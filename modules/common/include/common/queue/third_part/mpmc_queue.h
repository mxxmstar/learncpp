#pragma once

/// @file mpmc_queue.h
/// @brief 多生产者-多消费者无锁队列封装
///
/// 封装 moodycamel::ConcurrentQueue（third_part 目录下的开源库），
/// 提供与项目中 spsc_queue / mpsc_queue 一致的接口样式。
///
/// 两种变体：
/// - BoundedMpmcQueue<T>：  有界，满则 push 返回 false
/// - UnboundedMpmcQueue<T>：无界，可任意增长
///
/// 线程安全：多生产者 + 多消费者。

#include "common/queue/third_part/concurrentqueue.h"
#include "common/queue/third_part/blockingconcurrentqueue.h"

#include <cstddef>

namespace common::third_part {

/// @brief 有界 MPMC 无锁队列
/// @tparam T 元素类型
///
/// 基于 moodycamel::ConcurrentQueue，构造时分配初始容量。
/// push 满时返回 false，try_dequeue 空时返回 false。
/// 线程安全：多生产者 + 多消费者。
template<typename T>
class BoundedMpmcQueue {
public:
    /// @brief 构造函数
    /// @param capacity 队列初始容量
    explicit BoundedMpmcQueue(size_t capacity)
        : queue_(capacity), capacity_(capacity) {}

    /// @brief 推入元素（多生产者安全）
    /// @return true 成功，false 队列满
    bool push(const T& item) {
        return queue_.enqueue(item);
    }

    /// @brief 推入元素（移动语义）
    bool push(T&& item) {
        return queue_.enqueue(std::move(item));
    }

    /// @brief 弹出元素（多消费者安全）
    /// @param item 输出参数
    /// @return true 成功，false 队列空
    bool pop(T& item) {
        return queue_.try_dequeue(item);
    }

    /// @brief 队列是否为空（近似值）
    bool empty() const {
        return queue_.size_approx() == 0;
    }

    /// @brief 当前元素数量（近似值）
    size_t size() const {
        return queue_.size_approx();
    }

    /// @brief 获取队列容量
    size_t capacity() const {
        return capacity_;
    }

private:
    moodycamel::ConcurrentQueue<T> queue_;
    size_t capacity_;
};

/// @brief 无界 MPMC 无锁队列
/// @tparam T 元素类型
///
/// 基于 moodycamel::BlockingConcurrentQueue，
/// 支持阻塞等待的 pop 操作，永不拒绝 push。
template<typename T>
class UnboundedMpmcQueue {
public:
    UnboundedMpmcQueue() = default;

    explicit UnboundedMpmcQueue(size_t initial_capacity)
        : queue_(initial_capacity) {}

    /// @brief 推入元素（永不失败）
    void push(const T& item) {
        queue_.enqueue(item);
    }

    /// @brief 推入元素（移动语义）
    void push(T&& item) {
        queue_.enqueue(std::move(item));
    }

    /// @brief 非阻塞弹出
    /// @return true 成功，false 队列空
    bool try_pop(T& item) {
        return queue_.try_dequeue(item);
    }

    /// @brief 阻塞等待弹出
    /// @param item 输出参数
    void wait_pop(T& item) {
        queue_.wait_dequeue(item);
    }

    /// @brief 队列是否为空（近似值）
    bool empty() const {
        return queue_.size_approx() == 0;
    }

    /// @brief 当前元素数量（近似值）
    size_t size() const {
        return queue_.size_approx();
    }

private:
    moodycamel::BlockingConcurrentQueue<T> queue_;
};

} // namespace common::third_part
