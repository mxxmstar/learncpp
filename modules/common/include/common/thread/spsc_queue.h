#pragma once

/**
 * @file spsc_queue.h
 * @brief Single Producer Single Consumer 无锁队列
 *
 * 提供两种实现：
 *   - SpscQueue<T>：          有界（boost::lockfree::spsc_queue 包装），
 *                             构造时指定固定容量，满则 push 失败
 *   - UnboundedSpscQueue<T>： 无界（mutex + deque），
 *                             可任意增长，适合不允许丢包的场景
 */

#include <boost/lockfree/spsc_queue.hpp>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <cstddef>

namespace common {

// ============================================================================
// BoundedSpscQueue — 有界 SPSC 无锁队列
// 基于 boost::lockfree::spsc_queue，固定容量，满时 push 返回 false
// 线程安全：单生产者 + 单消费者
// ============================================================================
template<typename T>
class BoundedSpscQueue {
public:
    /// @brief 构造函数
    /// @param capacity 队列容量（必须是 2 的幂次，或由 boost 内部处理）
    explicit BoundedSpscQueue(size_t capacity)
        : capacity_(capacity)
        , queue_(capacity) {}

    /// @brief 推入元素（非阻塞）
    /// @param item 元素
    /// @return true 成功，false 队列已满
    bool push(const T& item) {
        return queue_.push(item);
    }

    /// @brief 推入元素（移动语义，非阻塞）
    bool push(T&& item) {
        return queue_.push(std::move(item));
    }

    /// @brief 弹出元素（非阻塞）
    /// @param item 输出参数
    /// @return true 成功，false 队列为空
    bool pop(T& item) {
        return queue_.pop(item);
    }

    /// @brief 队列是否为空
    bool empty() const {
        return const_cast<boost::lockfree::spsc_queue<T>&>(queue_).empty();
    }

    /// @brief 队列是否已满
    bool full() const {
        return const_cast<boost::lockfree::spsc_queue<T>&>(queue_).full();
    }

    /// @brief 当前可用读取数
    size_t size() const {
        return queue_.read_available();
    }

    /// @brief 当前可用写入数
    size_t available() const {
        return queue_.write_available();
    }

    /// @brief 获取队列容量
    size_t capacity() const {
        return capacity_;
    }

    /// @brief 清空队列
    void clear() {
        T item;
        while (queue_.pop(item)) {}
    }

private:
    size_t capacity_;
    boost::lockfree::spsc_queue<T> queue_;
};

// ============================================================================
// UnboundedSpscQueue — 无界 SPSC 队列
// 基于 std::deque + mutex，可无限增长，适合不允许丢包的场景
// 线程安全：单生产者 + 单消费者
// ============================================================================
template<typename T>
class UnboundedSpscQueue {
public:
    UnboundedSpscQueue() = default;

    /// @brief 推入元素（阻塞，永不失败）
    void push(const T& item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(item);
        }
        cv_.notify_one();
    }

    /// @brief 推入元素（移动语义）
    void push(T&& item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(item));
        }
        cv_.notify_one();
    }

    /// @brief 弹出元素（阻塞等待）
    /// @param item 输出参数
    /// @return true 成功，false 队列已关闭
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return !queue_.empty() || closed_; });
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    /// @brief 弹出元素（带超时）
    /// @param item 输出参数
    /// @param timeout 最大等待时间
    /// @return true 成功，false 超时或已关闭
    bool pop_for(T& item, const std::chrono::milliseconds& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this]() { return !queue_.empty() || closed_; })) {
            return false;
        }
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    /// @brief 非阻塞尝试弹出
    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    /// @brief 当前队列大小
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /// @brief 队列是否为空
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    /// @brief 清空队列
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
    }

    /// @brief 关闭队列（唤醒所有等待的消费者）
    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

    /// @brief 重置队列（清空 + 重新打开）
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
        closed_ = false;
    }

private:
    std::deque<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool closed_{false};
};

} // namespace common
