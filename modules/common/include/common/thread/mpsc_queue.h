#pragma once

/**
 * @file mpsc_queue.h
 * @brief Multiple Producer Single Consumer 无锁队列
 *
 * 提供两种实现：
 *   - BoundedMpscQueue<T>：  有界（boost::lockfree::queue 包装，固定节点池），
 *                            满则 push 返回 false
 *   - UnboundedMpscQueue<T>：无界（boost::lockfree::queue + fixed_sized<false>），
 *                            节点动态分配，永不拒绝
 */

#include <boost/lockfree/queue.hpp>
#include <atomic>
#include <cstddef>

namespace common {

// ============================================================================
// BoundedMpscQueue — 有界 MPSC/MPMC 无锁队列
// 基于 boost::lockfree::queue，构造时分配固定大小的节点池
// 线程安全：多生产者 + 单消费者
// ============================================================================
template<typename T>
class BoundedMpscQueue {
public:
    /// @brief 构造函数
    /// @param capacity 队列容量（节点池大小）
    explicit BoundedMpscQueue(size_t capacity)
        : queue_(capacity), size_(0) {}

    /// @brief 推入元素（多生产者安全）
    /// @param item 元素
    /// @return true 成功，false 队列满
    bool push(const T& item) {
        bool result = queue_.push(item);
        if (result) size_.fetch_add(1, std::memory_order_release);
        return result;
    }

    /// @brief 弹出元素（仅消费者线程调用）
    /// @param item 输出参数
    /// @return true 成功，false 队列空
    bool pop(T& item) {
        bool result = queue_.pop(item);
        if (result) size_.fetch_sub(1, std::memory_order_release);
        return result;
    }

    /// @brief 队列是否为空
    bool empty() const {
        return queue_.empty();
    }

    /// @brief 当前元素数量（近似值）
    size_t size() const {
        return size_.load(std::memory_order_acquire);
    }

    /// @brief 清空队列
    void clear() {
        T item;
        size_t popped = 0;
        while (queue_.pop(item)) { ++popped; }
        if (popped > 0) size_.fetch_sub(popped, std::memory_order_release);
    }

private:
    boost::lockfree::queue<T> queue_;
    std::atomic<size_t> size_;
};

// ============================================================================
// UnboundedMpscQueue — 无界 MPSC/MPMC 无锁队列
// 基于 boost::lockfree::queue + fixed_sized<false>，
// 节点池耗尽时自动动态分配，永不拒绝
// 线程安全：多生产者 + 单消费者
// ============================================================================
template<typename T>
class UnboundedMpscQueue {
public:
    /// @brief 构造函数
    /// @param initial_capacity 初始节点池大小（后续可动态增长）
    explicit UnboundedMpscQueue(size_t initial_capacity = 64)
        : queue_(initial_capacity), size_(0) {}

    /// @brief 推入元素（多生产者安全，永不失败）
    /// @param item 元素
    bool push(const T& item) {
        queue_.push(item);
        size_.fetch_add(1, std::memory_order_release);
        return true;
    }

    /// @brief 弹出元素（仅消费者线程调用）
    /// @param item 输出参数
    /// @return true 成功，false 队列空
    bool pop(T& item) {
        bool result = queue_.pop(item);
        if (result) size_.fetch_sub(1, std::memory_order_release);
        return result;
    }

    /// @brief 队列是否为空
    bool empty() const {
        return queue_.empty();
    }

    /// @brief 当前元素数量（近似值）
    size_t size() const {
        return size_.load(std::memory_order_acquire);
    }

    /// @brief 清空队列
    void clear() {
        T item;
        size_t popped = 0;
        while (queue_.pop(item)) { ++popped; }
        if (popped > 0) size_.fetch_sub(popped, std::memory_order_release);
    }

private:
    boost::lockfree::queue<T, boost::lockfree::fixed_sized<false>> queue_;
    std::atomic<size_t> size_;
};

} // namespace common
