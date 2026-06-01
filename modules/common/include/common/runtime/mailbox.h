#pragma once

/// @file mailbox.h
/// @brief Mailbox 聚合头文件
///
/// 包含 Mailbox 体系的所有组件：
/// - i_mailbox.h：枚举定义 + IMailBox 抽象接口
/// - spsc_mailbox.h：SPSCMailBox 实现（默认，无锁高性能）
/// - mpmc_mailbox.h：MPMCMailBox 实现（互斥锁保护）
///
/// 提供 CreateMailBox 工厂函数，根据 MailBoxKind 创建对应实例。
///
/// 使用方式：
/// @code
/// auto mb = CreateMailBox<int>(MailBoxKind::SPSC, 64);
/// mb->Push(42, BackpressurePolicy::DropOldest);
/// int val;
/// mb->TryPop(val);
/// @endcode

#include "common/runtime/i_mailbox.h"
#include "common/runtime/spsc_mailbox.h"
#include "common/runtime/mpmc_mailbox.h"

#include <memory>

namespace common::runtime {

/// @brief 创建 Mailbox 实例的工厂函数
/// @tparam T 元素类型
/// @param kind Mailbox 类型（SPSC 或 MPMC）
/// @param capacity 容量（0 表示无界）
/// @return Mailbox 实例的唯一指针
template <typename T>
std::unique_ptr<IMailBox<T>> CreateMailBox(MailBoxKind kind, std::size_t capacity) {
    if (kind == MailBoxKind::MPMC) {
        return std::make_unique<MPMCMailBox<T>>(capacity);
    }

    return std::make_unique<SPSCMailBox<T>>(capacity);
}

} // namespace common::runtime
