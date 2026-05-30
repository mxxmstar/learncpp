#include "common/thread/runtime_framework.h"

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
using common::thread::MpscExecutorTaskQueue;
using common::thread::NodeMetricsSnapshot;
using common::thread::NodeOptions;
using common::thread::Runtime;
using common::thread::SPSCMailBox;

class MultiplyNode : public INode<int> {
public:
    explicit MultiplyNode(int factor)
        : factor_(factor) {}

    void Process(int frame) override {
        Emit(frame * factor_);
    }

private:
    int factor_;
};

class CollectNode : public INode<int> {
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

class RangeSource : public ISourceNode<int> {
public:
    explicit RangeSource(int count)
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

static void TestMailboxDropOldest() {
    SPSCMailBox<int> mailbox(2);

    assert(mailbox.Push(1, BackpressurePolicy::DropOldest) ==
           common::thread::MailboxPushResult::Accepted);
    assert(mailbox.Push(2, BackpressurePolicy::DropOldest) ==
           common::thread::MailboxPushResult::Accepted);
    assert(mailbox.Push(3, BackpressurePolicy::DropOldest) ==
           common::thread::MailboxPushResult::DroppedOldest);

    int value = 0;
    assert(mailbox.TryPop(value));
    assert(value == 2);
    assert(mailbox.TryPop(value));
    assert(value == 3);
    assert(!mailbox.TryPop(value));
}

static void TestExecutorMpscQueue() {
    MpscExecutorTaskQueue queue;
    queue.Open();

    std::atomic<int> sum{0};
    std::vector<std::thread> producers;
    for (int producer = 0; producer < 4; ++producer) {
        producers.emplace_back([&queue, producer]() {
            for (int i = 0; i < 25; ++i) {
                queue.Push([producer, i]() {
                    (void)producer;
                    (void)i;
                });
            }
        });
    }

    int received = 0;
    common::thread::ExecutorTask task;
    while (received < 100) {
        if (queue.WaitPop(task)) {
            task();
            sum.fetch_add(1);
            ++received;
        }
    }

    for (auto& producer : producers) {
        producer.join();
    }

    queue.Close();
    assert(sum.load() == 100);
    assert(queue.Empty());
}

static void TestPassivePipeline() {
    Runtime<int> runtime;

    auto multiply = std::make_shared<MultiplyNode>(2);
    auto collect = std::make_shared<CollectNode>();

    assert(runtime.AddNode("multiply", multiply));
    assert(runtime.AddNode("collect", collect));
    assert(runtime.Connect("multiply", "collect"));
    assert(runtime.Start());

    assert(runtime.Push("multiply", 21));
    assert(collect->WaitForCount(1));

    auto values = collect->Values();
    assert(values.size() == 1);
    assert(values[0] == 42);

    NodeMetricsSnapshot metrics;
    assert(runtime.GetMetrics("multiply", metrics));
    assert(metrics.processed == 1);

    runtime.Stop();
}

static void TestActiveSource() {
    Runtime<int> runtime;
    NodeOptions options;
    options.backpressure = BackpressurePolicy::DropNewest;
    options.mailbox_capacity = 8;

    auto source = std::make_shared<RangeSource>(5);
    auto collect = std::make_shared<CollectNode>();

    assert(runtime.AddSource("source", source));
    assert(runtime.AddNode("collect", collect, options));
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

int main() {
    TestMailboxDropOldest();
    TestExecutorMpscQueue();
    TestPassivePipeline();
    TestActiveSource();
    return 0;
}
