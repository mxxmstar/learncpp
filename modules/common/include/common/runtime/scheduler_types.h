#pragma once

/// @file scheduler_types.h
/// @brief 调度器通用类型定义
///
/// 定义 NodeMetricsSnapshot / NodeMetrics / NodeOptions，
/// 这些类型被 asio 版和教学版的 scheduler 共享。

#include "common/runtime/i_mailbox.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace common::runtime {

/// @brief 节点指标快照（线程安全的一次性读取）
struct NodeMetricsSnapshot {
    std::uint64_t enqueued{0};   ///< 累计入队数
    std::uint64_t processed{0};  ///< 累计处理成功数
    std::uint64_t dropped{0};    ///< 累计丢弃数
    std::uint64_t rejected{0};   ///< 累计拒绝数（Mailbox 关闭后）
    std::uint64_t errors{0};     ///< 累计异常数
};

/// @brief 节点运行时指标（原子计数器）
struct NodeMetrics {
    std::atomic<std::uint64_t> enqueued{0};   ///< 入队计数器
    std::atomic<std::uint64_t> processed{0};  ///< 处理成功计数器
    std::atomic<std::uint64_t> dropped{0};    ///< 丢弃计数器
    std::atomic<std::uint64_t> rejected{0};   ///< 拒绝计数器
    std::atomic<std::uint64_t> errors{0};     ///< 异常计数器

    /// @brief 获取当前指标的原子快照
    NodeMetricsSnapshot Snapshot() const {
        return {
            enqueued.load(),
            processed.load(),
            dropped.load(),
            rejected.load(),
            errors.load()
        };
    }
};

/// @brief 节点配置选项
struct NodeOptions {
    std::string executor_name{"single"};                       ///< 绑定的执行器名称
    std::size_t mailbox_capacity{64};                          ///< Mailbox 容量
    std::size_t max_batch_size{64};                            ///< 每批最大处理帧数
    MailBoxKind mailbox_kind{MailBoxKind::SPSC};               ///< Mailbox 类型
    BackpressurePolicy backpressure{BackpressurePolicy::DropOldest}; ///< 背压策略
};

} // namespace common::runtime
