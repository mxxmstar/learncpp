#include "common/thread/asio_runtime_framework.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using common::thread::BackpressurePolicy;
using common::thread::INode;
using common::thread::ISourceNode;
using common::thread::NodeMetricsSnapshot;
using common::thread::NodeOptions;
using common::thread::asio::AsioRuntime;

class AsioMultiplyNode : public INode<int> {
public:
    explicit AsioMultiplyNode(int factor)
        : factor_(factor) {}

    void Process(int frame) override {
        Emit(frame * factor_);
    }

private:
    int factor_;
};

class AsioCollectNode : public INode<int> {
public:
    void Process(int frame) override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            values_.push_back(frame);
        }
        cv_.notify_all();
    }

    bool WaitForCount(std::size_t count) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(2), [&]() {
            return values_.size() >= count;
        });
    }

    std::vector<int> Values() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return values_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<int> values_;
};

class AsioRangeSource : public ISourceNode<int> {
public:
    explicit AsioRangeSource(int count)
        : count_(count) {}

    void Start() override {
        for (int i = 0; i < count_; ++i) {
            Emit(i);
        }
    }

    void Stop() override {}

private:
    int count_;
};

class NonConcurrentNode : public INode<int> {
public:
    void Process(int frame) override {
        int active = active_count_.fetch_add(1) + 1;
        if (active > 1) {
            saw_concurrency_.store(true);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        processed_.fetch_add(1);
        active_count_.fetch_sub(1);

        if (frame == expected_last_) {
            std::lock_guard<std::mutex> lock(mutex_);
            done_ = true;
            cv_.notify_all();
        }
    }

    void SetExpectedLast(int value) {
        expected_last_ = value;
    }

    bool WaitDone() {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(3), [&]() {
            return done_;
        });
    }

    bool SawConcurrency() const {
        return saw_concurrency_.load();
    }

    int Processed() const {
        return processed_.load();
    }

private:
    std::atomic<int> active_count_{0};
    std::atomic<int> processed_{0};
    std::atomic_bool saw_concurrency_{false};
    int expected_last_{0};
    std::mutex mutex_;
    std::condition_variable cv_;
    bool done_{false};
};

static void TestAsioPassivePipeline() {
    AsioRuntime<int> runtime;
    runtime.AddDefaultExecutors(2);

    NodeOptions cpu_options;
    cpu_options.executor_name = "cpu";
    cpu_options.backpressure = BackpressurePolicy::DropOldest;
    cpu_options.mailbox_capacity = 8;

    auto multiply = std::make_shared<AsioMultiplyNode>(3);
    auto collect = std::make_shared<AsioCollectNode>();

    assert(runtime.AddNode("multiply", multiply, cpu_options));
    assert(runtime.AddNode("collect", collect));
    assert(runtime.Connect("multiply", "collect"));
    assert(runtime.Start());

    assert(runtime.Push("multiply", 14));
    assert(collect->WaitForCount(1));

    auto values = collect->Values();
    assert(values.size() == 1);
    assert(values[0] == 42);

    NodeMetricsSnapshot metrics;
    assert(runtime.GetMetrics("multiply", metrics));
    assert(metrics.processed == 1);

    runtime.Stop();
}

static void TestAsioActiveSource() {
    AsioRuntime<int> runtime;

    auto source = std::make_shared<AsioRangeSource>(5);
    auto collect = std::make_shared<AsioCollectNode>();

    assert(runtime.AddSource("source", source));
    assert(runtime.AddNode("collect", collect));
    assert(runtime.Connect("source", "collect"));
    assert(runtime.Start());

    assert(collect->WaitForCount(5));
    auto values = collect->Values();
    assert(values.size() == 5);
    for (int i = 0; i < 5; ++i) {
        assert(values[static_cast<std::size_t>(i)] == i);
    }

    runtime.Stop();
}

static void TestAsioNodeSerializedOnThreadPool() {
    AsioRuntime<int> runtime;
    runtime.AddDefaultExecutors(4);

    NodeOptions options;
    options.executor_name = "cpu";
    options.mailbox_capacity = 64;
    options.max_batch_size = 4;
    options.backpressure = BackpressurePolicy::Block;

    auto node = std::make_shared<NonConcurrentNode>();
    node->SetExpectedLast(19);

    assert(runtime.AddNode("worker", node, options));
    assert(runtime.Start());

    for (int i = 0; i < 20; ++i) {
        assert(runtime.Push("worker", i));
    }

    assert(node->WaitDone());
    assert(node->Processed() == 20);
    assert(!node->SawConcurrency());

    runtime.Stop();
}

int main() {
    TestAsioPassivePipeline();
    TestAsioActiveSource();
    TestAsioNodeSerializedOnThreadPool();
    return 0;
}
