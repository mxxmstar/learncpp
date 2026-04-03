#pragma once

#include "video_pipeline/frame_data.h"
#include <boost/lockfree/spsc_queue.hpp>
#include <atomic>
#include <optional>
#include <chrono>
#include <cassert>

/// @brief 线程安全的无锁 SPSC（Single Producer Single Consumer）队列
/// @tparam T 数据类型
template<typename T>
class FrameQueue {
public:
    /// @brief 构造函数
    /// @param capacity 队列容量（必须是 2 的幂次）
    explicit FrameQueue(size_t capacity)
        : queue_(capacity)
        , shutdown_{false}
        , pushed_count_{0}
        , popped_count_{0} {}
    
    /// @brief 推入数据（非阻塞）
    /// @param item 要推入的数据
    /// @return true 成功，false 队列已满或已关闭
    bool push(T item) {
        if (shutdown_.load(std::memory_order_relaxed)) {
            return false;
        }
        
        bool success = queue_.push(std::move(item));
        if (success) {
            pushed_count_.fetch_add(1, std::memory_order_relaxed);
        }
        return success;
    }
    
    /// @brief 弹出数据（带超时等待）
    /// @param timeout 超时时间
    /// @return std::nullopt 表示超时或已关闭
    std::optional<T> pop(std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        
        while (!shutdown_.load(std::memory_order_relaxed)) {
            T item;
            if (queue_.pop(item)) {
                popped_count_.fetch_add(1, std::memory_order_relaxed);
                return std::optional<T>(std::move(item));
            }
            
            // 检查是否超时
            if (std::chrono::steady_clock::now() >= deadline) {
                break;
            }
            
            // 让出 CPU，避免忙等
            std::this_thread::yield();
        }
        return std::nullopt;
    }
    
    /// @brief 尝试弹出数据（不等待）
    /// @return std::nullopt 表示队列为空
    std::optional<T> try_pop() {
        T item;
        if (queue_.pop(item)) {
            popped_count_.fetch_add(1, std::memory_order_relaxed);
            return std::optional<T>(std::move(item));
        }
        return std::nullopt;
    }
    
    /// @brief 队列是否为空
    bool empty() const noexcept {
        return const_cast<boost::lockfree::spsc_queue<T>&>(queue_).empty();
    }
    
    /// @brief 队列是否已满
    bool full() const noexcept {
        return const_cast<boost::lockfree::spsc_queue<T>&>(queue_).full();
    }
    
    /// @brief 获取当前可读元素数量（近似值）
    size_t read_available() const noexcept {
        return queue_.read_available();
    }
    
    /// @brief 获取当前可写空间数量（近似值）
    size_t write_available() const noexcept {
        return queue_.write_available();
    }
    
    /// @brief 获取总推入数量
    size_t totalPushed() const noexcept {
        return pushed_count_.load(std::memory_order_relaxed);
    }
    
    /// @brief 获取总弹出数量
    size_t totalPopped() const noexcept {
        return popped_count_.load(std::memory_order_relaxed);
    }
    
    /// @brief 获取当前队列中的元素数量
    size_t size() const noexcept {
        return read_available();
    }
    
    /// @brief 关闭队列
    void shutdown() {
        shutdown_.store(true, std::memory_order_release);
    }
    
    /// @brief 是否已关闭
    bool isShutdown() const noexcept {
        return shutdown_.load(std::memory_order_acquire);
    }
    
    /// @brief 清空队列
    void clear() {
        T item;
        while (queue_.pop(item)) {
            // 丢弃所有元素
        }
    }
    
    /// @brief 重置队列状态
    void reset() {
        clear();
        shutdown_.store(false, std::memory_order_release);
        pushed_count_.store(0, std::memory_order_relaxed);
        popped_count_.store(0, std::memory_order_relaxed);
    }
    
private:
    // 使用动态容量的 SPSC 队列
    boost::lockfree::spsc_queue<T> queue_;
    
    // 关闭标志
    std::atomic<bool> shutdown_;
    
    // 统计信息
    std::atomic<size_t> pushed_count_;
    std::atomic<size_t> popped_count_;
};

/// @brief 帧队列类型别名
using FrameDataQueue = FrameQueue<FrameData>;
using RawPacketQueue = FrameQueue<RawPacketData>;
