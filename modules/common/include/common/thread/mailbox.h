#pragma once

#include "common/thread/spsc_queue.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <utility>

namespace common::thread {

enum class BackpressurePolicy {
    Block,
    DropNewest,
    DropOldest,
    Unbounded
};

enum class MailboxPushResult {
    Accepted,
    DroppedNewest,
    DroppedOldest,
    Closed
};

enum class MailBoxKind {
    SPSC,
    MPMC
};

template <typename T>
class IMailBox {
public:
    virtual ~IMailBox() = default;

    virtual MailboxPushResult Push(T item, BackpressurePolicy policy) = 0;
    virtual bool TryPop(T& item) = 0;
    virtual bool WaitPop(T& item) = 0;
    virtual void Close() = 0;
    virtual void Open() = 0;
    virtual void Clear() = 0;
    virtual bool Empty() const = 0;
    virtual std::size_t Size() const = 0;
    virtual std::size_t Capacity() const = 0;
    virtual bool IsClosed() const = 0;
};

template <typename T>
class SPSCMailBox : public IMailBox<T> {
public:
    explicit SPSCMailBox(std::size_t capacity = 64)
        : capacity_(capacity)
        , bounded_(capacity == 0 ? nullptr : std::make_unique<BoundedSpscQueue<T>>(capacity))
        , unbounded_(capacity == 0 ? std::make_unique<UnboundedSpscQueue<T>>() : nullptr) {}

    SPSCMailBox(const SPSCMailBox&) = delete;
    SPSCMailBox& operator=(const SPSCMailBox&) = delete;

    MailboxPushResult Push(T item, BackpressurePolicy policy) override {
        if (closed_.load()) {
            return MailboxPushResult::Closed;
        }

        if (unbounded_) {
            unbounded_->push(std::move(item));
            data_cv_.notify_one();
            return MailboxPushResult::Accepted;
        }

        if (policy == BackpressurePolicy::Block || policy == BackpressurePolicy::Unbounded) {
            while (!closed_.load()) {
                if (!bounded_->full() && bounded_->push(std::move(item))) {
                    data_cv_.notify_one();
                    return MailboxPushResult::Accepted;
                }

                std::unique_lock<std::mutex> lock(wait_mutex_);
                space_cv_.wait(lock, [this]() {
                    return closed_.load() || !bounded_->full();
                });
            }

            return MailboxPushResult::Closed;
        }

        if (!bounded_->full() && bounded_->push(std::move(item))) {
            data_cv_.notify_one();
            return MailboxPushResult::Accepted;
        }

        if (policy == BackpressurePolicy::DropOldest) {
            T dropped{};
            if (bounded_->pop(dropped)) {
                if (bounded_->push(std::move(item))) {
                    data_cv_.notify_one();
                    space_cv_.notify_one();
                    return MailboxPushResult::DroppedOldest;
                }
            }
        }

        return MailboxPushResult::DroppedNewest;
    }

    bool TryPop(T& item) override {
        bool popped = unbounded_ ? unbounded_->try_pop(item) : bounded_->pop(item);
        if (popped) {
            space_cv_.notify_one();
        }
        return popped;
    }

    bool WaitPop(T& item) override {
        while (true) {
            if (TryPop(item)) {
                return true;
            }

            if (closed_.load()) {
                return false;
            }

            std::unique_lock<std::mutex> lock(wait_mutex_);
            data_cv_.wait(lock, [this]() {
                return closed_.load() || !Empty();
            });
        }
    }

    void Close() override {
        closed_.store(true);
        if (unbounded_) {
            unbounded_->close();
        }
        data_cv_.notify_all();
        space_cv_.notify_all();
    }

    void Open() override {
        closed_.store(false);
        if (unbounded_) {
            unbounded_->reset();
        }
    }

    void Clear() override {
        if (unbounded_) {
            unbounded_->clear();
        } else {
            bounded_->clear();
        }
        space_cv_.notify_all();
    }

    bool Empty() const override {
        return unbounded_ ? unbounded_->empty() : bounded_->empty();
    }

    std::size_t Size() const override {
        return unbounded_ ? unbounded_->size() : bounded_->size();
    }

    std::size_t Capacity() const override {
        return capacity_;
    }

    bool IsClosed() const override {
        return closed_.load();
    }

private:
    std::size_t capacity_;
    std::unique_ptr<BoundedSpscQueue<T>> bounded_;
    std::unique_ptr<UnboundedSpscQueue<T>> unbounded_;
    std::atomic_bool closed_{false};
    mutable std::mutex wait_mutex_;
    std::condition_variable data_cv_;
    std::condition_variable space_cv_;
};

template <typename T>
class MPMCMailBox : public IMailBox<T> {
public:
    explicit MPMCMailBox(std::size_t capacity = 64)
        : capacity_(capacity) {}

    MPMCMailBox(const MPMCMailBox&) = delete;
    MPMCMailBox& operator=(const MPMCMailBox&) = delete;

    MailboxPushResult Push(T item, BackpressurePolicy policy) override {
        std::unique_lock<std::mutex> lock(mutex_);

        if (closed_) {
            return MailboxPushResult::Closed;
        }

        if (policy == BackpressurePolicy::Unbounded || capacity_ == 0) {
            queue_.push_back(std::move(item));
            cv_.notify_one();
            return MailboxPushResult::Accepted;
        }

        if (policy == BackpressurePolicy::Block) {
            cv_.wait(lock, [this]() {
                return closed_ || queue_.size() < capacity_;
            });

            if (closed_) {
                return MailboxPushResult::Closed;
            }

            queue_.push_back(std::move(item));
            cv_.notify_one();
            return MailboxPushResult::Accepted;
        }

        if (queue_.size() < capacity_) {
            queue_.push_back(std::move(item));
            cv_.notify_one();
            return MailboxPushResult::Accepted;
        }

        if (policy == BackpressurePolicy::DropOldest) {
            queue_.pop_front();
            queue_.push_back(std::move(item));
            cv_.notify_one();
            return MailboxPushResult::DroppedOldest;
        }

        return MailboxPushResult::DroppedNewest;
    }

    bool TryPop(T& item) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }

        item = std::move(queue_.front());
        queue_.pop_front();
        cv_.notify_one();
        return true;
    }

    bool WaitPop(T& item) override {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() {
            return closed_ || !queue_.empty();
        });

        if (queue_.empty()) {
            return false;
        }

        item = std::move(queue_.front());
        queue_.pop_front();
        cv_.notify_one();
        return true;
    }

    void Close() override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

    void Open() override {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = false;
    }

    void Clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
        cv_.notify_all();
    }

    bool Empty() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    std::size_t Size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    std::size_t Capacity() const override {
        return capacity_;
    }

    bool IsClosed() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

private:
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<T> queue_;
    bool closed_{false};
};

template <typename T>
std::unique_ptr<IMailBox<T>> CreateMailBox(MailBoxKind kind, std::size_t capacity) {
    if (kind == MailBoxKind::MPMC) {
        return std::make_unique<MPMCMailBox<T>>(capacity);
    }

    return std::make_unique<SPSCMailBox<T>>(capacity);
}

} // namespace common::thread
