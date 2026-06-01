#pragma once

/// @file asio_runtime_framework.h
/// @brief 基于 Asio 的生产级运行时框架聚合头文件
///
/// 使用方式：
/// @code
/// #include "common/runtime/asio/asio_runtime_framework.h"
/// common::runtime::asio::AsioRuntime<Frame> rt;
/// @endcode

#include "common/runtime/asio/asio_executor.h"
#include "common/runtime/asio/asio_runtime.h"
#include "common/runtime/asio/asio_scheduler.h"
#include "common/runtime/mailbox.h"
#include "common/runtime/node.h"
#include "common/runtime/scheduler_types.h"
