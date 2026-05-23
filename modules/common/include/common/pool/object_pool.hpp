#pragma once

#include <boost/pool/pool.hpp>
#include <cstring>
#include <mutex>

/// @brief 轻量级对象池，基于 boost::pool
///
/// 提供两种接口风格：
///   - Allocate() / Deallocate()  —— 适合 C 结构体（零初始化、无构造/析构）
///   - Acquire()  / Release()     —— 适合 C++ 对象（placement-new 构造、显式析构）
template <typename T>
class ObjectPool {
public:
    /// @param initial_chunks 首次分配的块数
    explicit ObjectPool(std::size_t initial_chunks = 64)
        : pool_(sizeof(T), initial_chunks) {}

    ~ObjectPool() = default;
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) noexcept = default;
    ObjectPool& operator=(ObjectPool&&) noexcept = default;

    /// @brief 分配一块零初始化的 T 内存（适合 C 结构体）
    T* Allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        void* mem = pool_.malloc();
        if (!mem) return nullptr;
        std::memset(mem, 0, sizeof(T));
        return static_cast<T*>(mem);
    }

    /// @brief 归还 Allocate() 获得的内存（不调用析构函数）
    void Deallocate(T* obj) {
        if (!obj) return;
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.free(obj);
    }

    /// @brief 从池中构造一个 T 对象
    template <typename... Args>
    T* Acquire(Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        void* mem = pool_.malloc();
        if (!mem) return nullptr;
        return new (mem) T(std::forward<Args>(args)...);
    }

    /// @brief 析构并归还 Acquire() 获得的对象
    void Release(T* obj) {
        if (!obj) return;
        obj->~T();
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.free(obj);
    }

private:
    boost::pool<> pool_;
    std::mutex    mutex_;
};
