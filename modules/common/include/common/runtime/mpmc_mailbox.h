#pragma once

/// @file mpmc_mailbox.h
/// @brief 多生产者-多消费者 Mailbox 实现
///
/// 兼容原 Mailbox<T> 的语义，使用 std::deque + std::mutex 实现。
/// 所有操作均有锁保护。仅在确实需要多生产者并发入队时使用。

#include "common/runtime/i_mailbox.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

namespace common::runtime {

/// @brief 多生产者-多消费者 Mailbox 实现
/// @tparam T 元素类型
template <typename T>
class MPMCMailBox : public IMailBox<T> {
public:
    /// @brief 构造函数
    /// @param capacity 容量（0 表示无界）
    explicit MPMCMailBox(std::size_t capacity = 64)
        : capacity_(capacity) {}

    MPMCMailBox(const MPMCMailBox&) = delete;
    MPMCMailBox& operator=(const MPMCMailBox&) = delete;

    /// @brief 推入元素（有锁）
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

} // namespace common::runtime
