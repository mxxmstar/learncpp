#pragma once

/// @file runtime_framework.h
/// @brief 教学版运行时框架聚合头文件
///
/// 包含教学版运行时所需的所有组件。
/// 使用方式：
/// @code
/// #include "common/runtime/__example/runtime_framework.h"
/// common::runtime::Runtime<Frame> rt;
/// @endcode

#include "common/runtime/__example/executor.h"
#include "common/runtime/mailbox.h"
#include "common/runtime/node.h"
#include "common/runtime/__example/runtime.h"
#include "common/runtime/__example/scheduler.h"
