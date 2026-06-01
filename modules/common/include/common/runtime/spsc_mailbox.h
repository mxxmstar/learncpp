#pragma once

/// @file spsc_mailbox.h
/// @brief 单生产者-单消费者 Mailbox 实现
///
/// 默认的 Mailbox 实现，性能最优：
/// - DropNewest/DropOldest 路径：使用 BoundedSpscQueue（无锁环形缓冲区）
/// - Unbounded 模式（容量为 0）：使用 UnboundedSpscQueue
/// - Block 策略：自旋等待 + 条件变量

#include "common/runtime/i_mailbox.h"
#include "common/queue/spsc_queue.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <utility>

namespace common::runtime {

/// @brief 单生产者-单消费者 Mailbox 实现
/// @tparam T 元素类型
template <typename T>
class SPSCMailBox : public IMailBox<T> {
public:
    /// @brief 构造函数
    /// @param capacity 容量（0 表示无界）
    explicit SPSCMailBox(std::size_t capacity = 64)
        : capacity_(capacity)
        , bounded_(capacity == 0 ? nullptr : std::make_unique<BoundedSpscQueue<T>>(capacity))
        , unbounded_(capacity == 0 ? std::make_unique<UnboundedSpscQueue<T>>() : nullptr) {}

    SPSCMailBox(const SPSCMailBox&) = delete;
    SPSCMailBox& operator=(const SPSCMailBox&) = delete;

    /// @brief 推入元素
    /// @param item 元素
    /// @param policy 背压策略
    /// @return 操作结果
    ///
    /// 对 DropNewest/DropOldest 使用无锁路径，对 Block 使用条件变量等待。
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

    /// @brief 非阻塞弹出
    bool TryPop(T& item) override {
        bool popped = unbounded_ ? unbounded_->try_pop(item) : bounded_->pop(item);
        if (popped) {
            space_cv_.notify_one();
        }
        return popped;
    }

    /// @brief 阻塞等待弹出
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

} // namespace common::runtime
