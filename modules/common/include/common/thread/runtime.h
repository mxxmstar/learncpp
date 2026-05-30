#pragma once

#include "common/thread/executor.h"
#include "common/thread/node.h"
#include "common/thread/scheduler.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace common::thread {

template <typename Frame>
class Runtime {
public:
    using NodeId = std::string;
    using Context = NodeContext<Frame>;
    using SourcePtr = std::shared_ptr<ISourceNode<Frame>>;
    using NodePtr = std::shared_ptr<INode<Frame>>;

    Runtime() {
        AddExecutor(std::make_unique<SingleThreadExecutor>("single"));
    }

    ~Runtime() {
        Stop();
    }

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    bool AddExecutor(std::unique_ptr<IExecutor> executor) {
        if (!executor) {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            return false;
        }

        auto name = executor->Name();
        return executors_.emplace(std::move(name), std::move(executor)).second;
    }

    void AddDefaultExecutors(std::size_t cpu_threads = std::thread::hardware_concurrency()) {
        AddExecutor(std::make_unique<ThreadPoolExecutor>(
            "cpu", cpu_threads == 0 ? 1 : cpu_threads));
        AddExecutor(std::make_unique<InferenceExecutor>("inference", 1));
        AddExecutor(std::make_unique<IOExecutor>("io", 1));
    }

    bool AddNode(NodeId id, NodePtr node, NodeOptions options = {}) {
        if (!node) {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (running_ || nodes_.find(id) != nodes_.end() || sources_.find(id) != sources_.end()) {
            return false;
        }

        auto executor = FindExecutorLocked(options.executor_name);
        if (!executor) {
            return false;
        }

        auto raw_id = id;
        node->SetEmitCallback([this, raw_id](Frame frame) {
            Emit(raw_id, std::move(frame));
        });

        auto context = std::make_unique<Context>(
            std::move(id), std::move(node), executor, std::move(options));
        nodes_.emplace(context->id, std::move(context));
        return true;
    }

    bool AddSource(NodeId id, SourcePtr source) {
        if (!source) {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (running_ || nodes_.find(id) != nodes_.end() || sources_.find(id) != sources_.end()) {
            return false;
        }

        auto raw_id = id;
        source->SetEmitCallback([this, raw_id](Frame frame) {
            Emit(raw_id, std::move(frame));
        });

        sources_.emplace(std::move(id), std::move(source));
        return true;
    }

    bool Connect(const NodeId& from, const NodeId& to) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_ || !HasProducerLocked(from) || nodes_.find(to) == nodes_.end()) {
            return false;
        }

        auto& downstream = edges_[from];
        if (std::find(downstream.begin(), downstream.end(), to) != downstream.end()) {
            return true;
        }

        downstream.push_back(to);
        return true;
    }

    bool Start() {
        std::vector<SourcePtr> sources;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (running_) {
                return false;
            }

            for (auto& [_, context] : nodes_) {
                context->mailbox->Open();
            }

            for (auto& [_, executor] : executors_) {
                executor->Start();
            }

            running_ = true;
            for (auto& [_, source] : sources_) {
                sources.push_back(source);
            }
        }

        for (auto& source : sources) {
            source->Start();
        }

        return true;
    }

    void Stop() {
        std::vector<SourcePtr> sources;
        std::vector<IExecutor*> executors;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                return;
            }

            running_ = false;

            for (auto& [_, source] : sources_) {
                sources.push_back(source);
            }

            for (auto& [_, context] : nodes_) {
                context->mailbox->Close();
            }

            for (auto& [_, executor] : executors_) {
                executors.push_back(executor.get());
            }
        }

        for (auto& source : sources) {
            source->Stop();
        }

        for (auto* executor : executors) {
            executor->Stop();
        }
    }

    bool Push(const NodeId& to, Frame frame) {
        Context* context = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                return false;
            }

            auto it = nodes_.find(to);
            if (it == nodes_.end()) {
                return false;
            }
            context = it->second.get();
        }

        return scheduler_.Enqueue(*context, std::move(frame));
    }

    bool Emit(const NodeId& from, Frame frame) {
        std::vector<Context*> targets;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                return false;
            }

            auto edge_it = edges_.find(from);
            if (edge_it == edges_.end()) {
                return false;
            }

            for (const auto& to : edge_it->second) {
                auto node_it = nodes_.find(to);
                if (node_it != nodes_.end()) {
                    targets.push_back(node_it->second.get());
                }
            }
        }

        bool accepted = false;
        for (auto* target : targets) {
            accepted = scheduler_.Enqueue(*target, frame) || accepted;
        }
        return accepted;
    }

    bool GetMetrics(const NodeId& id, NodeMetricsSnapshot& out) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodes_.find(id);
        if (it == nodes_.end()) {
            return false;
        }

        out = it->second->metrics.Snapshot();
        return true;
    }

    void SetErrorHandler(typename Scheduler<Frame>::ErrorHandler handler) {
        scheduler_.SetErrorHandler(std::move(handler));
    }

private:
    IExecutor* FindExecutorLocked(const std::string& name) const {
        auto it = executors_.find(name);
        return it == executors_.end() ? nullptr : it->second.get();
    }

    bool HasProducerLocked(const NodeId& id) const {
        return nodes_.find(id) != nodes_.end() || sources_.find(id) != sources_.end();
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<IExecutor>> executors_;
    std::unordered_map<NodeId, std::unique_ptr<Context>> nodes_;
    std::unordered_map<NodeId, SourcePtr> sources_;
    std::unordered_map<NodeId, std::vector<NodeId>> edges_;
    Scheduler<Frame> scheduler_;
    bool running_{false};
};

} // namespace common::thread
