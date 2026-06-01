#pragma once

/// @file asio_scheduler.h
/// @brief 基于 Asio 的生产级调度器
///
/// 与教学版 Scheduler 功能相同，但使用 strand 确保 Drain 串行执行。
/// 采用两层 Post 机制：
/// 1. 外层 Post 计入执行器的 pending 计数
/// 2. 内层 post(strand) 确保串行化

#include "common/runtime/asio/asio_executor.h"
#include "common/runtime/scheduler_types.h"
#include "common/runtime/mailbox.h"
#include "common/runtime/node.h"

#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>

#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace common::runtime::asio {

/// @brief 基于 Asio 的节点上下文
template <typename Frame>
struct AsioNodeContext {
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

    AsioNodeContext(std::string node_id,
                    std::shared_ptr<INode<Frame>> node_instance,
                    AsioExecutor* bound_executor,
                    NodeOptions node_options)
        : id(std::move(node_id))
        , node(std::move(node_instance))
        , executor(bound_executor)
        , options(std::move(node_options))
        , mailbox(CreateMailBox<Frame>(
              options.mailbox_kind,
              options.backpressure == BackpressurePolicy::Unbounded ? 0 : options.mailbox_capacity))
        , strand(executor->GetIOContext().get_executor()) {}

    std::string id;
    std::shared_ptr<INode<Frame>> node;
    AsioExecutor* executor{nullptr};
    NodeOptions options;
    std::unique_ptr<IMailBox<Frame>> mailbox;
    NodeMetrics metrics;
    std::atomic_bool scheduled{false};
    Strand strand;
};

/// @brief 基于 Asio 的调度器
template <typename Frame>
class AsioScheduler {
public:
    using Context = AsioNodeContext<Frame>;
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
        if (!ctx || !ctx->node || !ctx->executor) return;

        bool expected = false;
        if (!ctx->scheduled.compare_exchange_strong(expected, true)) return;

        if (!ctx->executor->Post([this, ctx]() {
                boost::asio::post(ctx->strand, [this, ctx]() { Drain(ctx); });
            })) {
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
                if (error_handler_) error_handler_(ctx->id, std::current_exception());
            }

            ++processed_in_batch;
            if (ctx->options.max_batch_size != 0 &&
                processed_in_batch >= ctx->options.max_batch_size) break;
        }

        ctx->scheduled.store(false);
        if (!ctx->mailbox->Empty()) Schedule(ctx);
    }

    ErrorHandler error_handler_;
};

} // namespace common::runtime::asio
