#pragma once

/// @file i_mailbox.h
/// @brief Mailbox 抽象接口和枚举定义
///
/// 定义 IMailBox<T> 纯虚接口以及背压策略、Push 结果、Mailbox 类型枚举。
/// 具体实现见 spsc_mailbox.h（单生产者-单消费者，默认）
/// 和 mpmc_mailbox.h（多生产者-多消费者）。

#include <cstddef>

namespace common::runtime {

/// @brief 背压策略枚举
enum class BackpressurePolicy {
    Block,        ///< 阻塞生产者直到队列有空位
    DropNewest,   ///< 丢弃新到达的数据
    DropOldest,   ///< 丢弃队列中最旧的数据
    Unbounded     ///< 无限制增长（内存无上限）
};

/// @brief Push 操作结果枚举
enum class MailboxPushResult {
    Accepted,       ///< 成功入队
    DroppedNewest,  ///< 丢弃了新数据（队列满 + DropNewest 策略）
    DroppedOldest,  ///< 丢弃了最旧数据（队列满 + DropOldest 策略）
    Closed          ///< 队列已关闭，拒绝入队
};

/// @brief Mailbox 类型枚举
enum class MailBoxKind {
    SPSC,  ///< 单生产者-单消费者（默认，无锁高性能路径）
    MPMC   ///< 多生产者-多消费者（互斥锁保护）
};

/// @brief Mailbox 抽象接口
/// @tparam T 元素类型
///
/// 定义线程间异步通信通道的通用操作。
/// 具体实现由 SPSCMailBox 和 MPMCMailBox 提供。
template <typename T>
class IMailBox {
public:
    virtual ~IMailBox() = default;

    /// @brief 推入元素
    /// @param item 元素
    /// @param policy 背压策略
    /// @return 操作结果（Accepted / DroppedNewest / DroppedOldest / Closed）
    virtual MailboxPushResult Push(T item, BackpressurePolicy policy) = 0;

    /// @brief 非阻塞弹出
    /// @param item 输出参数
    /// @return true 成功，false 队列空
    virtual bool TryPop(T& item) = 0;

    /// @brief 阻塞等待弹出
    /// @param item 输出参数
    /// @return true 成功，false 队列已关闭
    virtual bool WaitPop(T& item) = 0;

    /// @brief 关闭（唤醒所有等待者）
    virtual void Close() = 0;

    /// @brief 重新打开
    virtual void Open() = 0;

    /// @brief 清空
    virtual void Clear() = 0;

    /// @brief 队列是否为空
    virtual bool Empty() const = 0;

    /// @brief 当前元素数量
    virtual std::size_t Size() const = 0;

    /// @brief 队列容量（0 表示无界）
    virtual std::size_t Capacity() const = 0;

    /// @brief 队列是否已关闭
    virtual bool IsClosed() const = 0;
};

} // namespace common::runtime
