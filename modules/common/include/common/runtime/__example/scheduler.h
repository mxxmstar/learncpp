#pragma once

/// @file scheduler.h
/// @brief 教学版调度器
///
/// 定义 NodeContext（节点上下文）和 Scheduler（调度器）模板。
///
/// 核心职责：
/// 1. Enqueue：将帧推入节点的 Mailbox
/// 2. Schedule：通过 CAS 标记将 Drain 任务投递到 Executor
/// 3. Drain：批量从 Mailbox 取出帧并调用 Node::Process
/// 4. 自调度：Drain 结束后若还有数据，递归触发下一轮 Schedule

#include "common/runtime/scheduler_types.h"
#include "common/runtime/mailbox.h"
#include "common/runtime/node.h"
#include "common/runtime/__example/executor.h"

#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace common::runtime {

/// @brief 节点上下文
/// @tparam Frame 数据帧类型
///
/// 维护单个节点在运行时所需的所有状态：
/// - 节点实例、绑定的执行器、配置选项
/// - 输入 Mailbox（由 CreateMailBox 工厂创建）
/// - 运行时指标和调度标记
template <typename Frame>
struct NodeContext {
    /// @brief 构造函数
    /// @param node_id 节点唯一标识
    /// @param node_instance 节点实例
    /// @param bound_executor 绑定的执行器
    /// @param node_options 节点配置选项
    NodeContext(std::string node_id,
                std::shared_ptr<INode<Frame>> node_instance,
                IExecutor* bound_executor,
                NodeOptions node_options)
        : id(std::move(node_id))
        , node(std::move(node_instance))
        , executor(bound_executor)
        , options(std::move(node_options))
        , mailbox(CreateMailBox<Frame>(
              options.mailbox_kind,
              options.backpressure == BackpressurePolicy::Unbounded ? 0 : options.mailbox_capacity)) {}

    std::string id;                                     ///< 节点唯一标识
    std::shared_ptr<INode<Frame>> node;                 ///< 节点实例
    IExecutor* executor{nullptr};                       ///< 绑定的执行器
    NodeOptions options;                                ///< 节点配置选项
    std::unique_ptr<IMailBox<Frame>> mailbox;           ///< 输入队列
    NodeMetrics metrics;                                ///< 运行时指标
    std::atomic_bool scheduled{false};                  ///< 是否已提交到 Executor（防重复调度）
};

/// @brief 调度器
/// @tparam Frame 数据帧类型
template <typename Frame>
class Scheduler {
public:
    using Context = NodeContext<Frame>;
    using ErrorHandler = std::function<void(const std::string&, std::exception_ptr)>;

    void SetErrorHandler(ErrorHandler handler) {
        error_handler_ = std::move(handler);
    }

    bool Enqueue(Context& ctx, Frame frame) {
        auto result = ctx.mailbox->Push(std::move(frame), ctx.options.backpressure);

        switch (result) {
        case MailboxPushResult::Accepted:
            ctx.metrics.enqueued.fetch_add(1);
            break;
        case MailboxPushResult::DroppedOldest:
            ctx.metrics.enqueued.fetch_add(1);
            ctx.metrics.dropped.fetch_add(1);
            break;
        case MailboxPushResult::DroppedNewest:
            ctx.metrics.dropped.fetch_add(1);
            return false;
        case MailboxPushResult::Closed:
            ctx.metrics.rejected.fetch_add(1);
            return false;
        }

        Schedule(&ctx);
        return true;
    }

    void Schedule(Context* ctx) {
        if (!ctx || !ctx->node || !ctx->executor) {
            return;
        }

        bool expected = false;
        if (!ctx->scheduled.compare_exchange_strong(expected, true)) {
            return;
        }

        if (!ctx->executor->Post([this, ctx]() { Drain(ctx); })) {
            ctx->scheduled.store(false);
            ctx->metrics.rejected.fetch_add(1);
        }
    }

private:
    void Drain(Context* ctx) {
        std::size_t processed_in_batch = 0;
        Frame frame{};

        while (ctx->mailbox->TryPop(frame)) {
            try {
                ctx->node->Process(std::move(frame));
                ctx->metrics.processed.fetch_add(1);
            } catch (...) {
                ctx->metrics.errors.fetch_add(1);
                if (error_handler_) {
                    error_handler_(ctx->id, std::current_exception());
                }
            }

            ++processed_in_batch;
            if (ctx->options.max_batch_size != 0 &&
                processed_in_batch >= ctx->options.max_batch_size) {
                break;
            }
        }

        ctx->scheduled.store(false);

        if (!ctx->mailbox->Empty()) {
            Schedule(ctx);
        }
    }

    ErrorHandler error_handler_;
};

} // namespace common::runtime
