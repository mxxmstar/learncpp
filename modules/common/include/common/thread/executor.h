#pragma once

#include "common/thread/mpsc_queue.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace common::thread {

using ExecutorTask = std::function<void()>;

class IExecutorTaskQueue {
public:
    virtual ~IExecutorTaskQueue() = default;

    virtual bool Push(ExecutorTask task) = 0;
    virtual bool TryPop(ExecutorTask& task) = 0;
    virtual bool WaitPop(ExecutorTask& task) = 0;
    virtual void Close() = 0;
    virtual void Open() = 0;
    virtual void Clear() = 0;
    virtual bool Empty() const = 0;
    virtual std::size_t Size() const = 0;
};

class MpscExecutorTaskQueue : public IExecutorTaskQueue {
public:
    MpscExecutorTaskQueue() = default;

    ~MpscExecutorTaskQueue() override {
        Close();
        Clear();
    }

    MpscExecutorTaskQueue(const MpscExecutorTaskQueue&) = delete;
    MpscExecutorTaskQueue& operator=(const MpscExecutorTaskQueue&) = delete;

    bool Push(ExecutorTask task) override {
        if (closed_.load()) {
            return false;
        }

        auto* stored = new ExecutorTask(std::move(task));
        queue_.push(stored);
        cv_.notify_one();
        return true;
    }

    bool TryPop(ExecutorTask& task) override {
        ExecutorTask* stored = nullptr;
        if (!queue_.pop(stored)) {
            return false;
        }

        std::unique_ptr<ExecutorTask> holder(stored);
        task = std::move(*holder);
        return true;
    }

    bool WaitPop(ExecutorTask& task) override {
        while (true) {
            if (TryPop(task)) {
                return true;
            }

            if (closed_.load()) {
                return false;
            }

            std::unique_lock<std::mutex> lock(wait_mutex_);
            cv_.wait(lock, [this]() {
                return closed_.load() || !queue_.empty();
            });
        }
    }

    void Close() override {
        closed_.store(true);
        cv_.notify_all();
    }

    void Open() override {
        closed_.store(false);
    }

    void Clear() override {
        ExecutorTask* stored = nullptr;
        while (queue_.pop(stored)) {
            delete stored;
        }
    }

    bool Empty() const override {
        return queue_.empty();
    }

    std::size_t Size() const override {
        return queue_.size();
    }

private:
    common::UnboundedMpscQueue<ExecutorTask*> queue_;
    std::atomic_bool closed_{false};
    mutable std::mutex wait_mutex_;
    std::condition_variable cv_;
};

class IExecutor {
public:
    using Task = ExecutorTask;

    virtual ~IExecutor() = default;

    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual bool Post(Task task) = 0;
    virtual const std::string& Name() const = 0;
    virtual std::size_t Pending() const = 0;
};

class ThreadPoolExecutor : public IExecutor {
public:
    explicit ThreadPoolExecutor(std::string name, std::size_t thread_count = 1)
        : name_(std::move(name))
        , thread_count_(thread_count == 0 ? 1 : thread_count) {}

    ~ThreadPoolExecutor() override {
        Stop();
    }

    ThreadPoolExecutor(const ThreadPoolExecutor&) = delete;
    ThreadPoolExecutor& operator=(const ThreadPoolExecutor&) = delete;

    void Start() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            return;
        }

        stopping_ = false;
        running_ = true;
        next_queue_.store(0);

        task_queues_.clear();
        task_queues_.reserve(thread_count_);
        workers_.reserve(thread_count_);

        for (std::size_t i = 0; i < thread_count_; ++i) {
            auto queue = std::make_unique<MpscExecutorTaskQueue>();
            queue->Open();
            auto* raw_queue = queue.get();
            task_queues_.push_back(std::move(queue));
            workers_.emplace_back([this, raw_queue]() { WorkerLoop(*raw_queue); });
        }
    }

    void Stop() override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                return;
            }

            stopping_ = true;
            for (auto& queue : task_queues_) {
                queue->Close();
            }
        }

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        workers_.clear();
        task_queues_.clear();
        running_ = false;
        stopping_ = false;
    }

    bool Post(Task task) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ || stopping_ || task_queues_.empty()) {
            return false;
        }

        auto index = next_queue_.fetch_add(1) % task_queues_.size();
        return task_queues_[index]->Push(std::move(task));
    }

    const std::string& Name() const override {
        return name_;
    }

    std::size_t Pending() const override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::size_t pending = active_tasks_.load();
        for (const auto& queue : task_queues_) {
            pending += queue->Size();
        }
        return pending;
    }

protected:
    void WorkerLoop(IExecutorTaskQueue& queue) {
        Task task;
        while (queue.WaitPop(task)) {
            active_tasks_.fetch_add(1);

            try {
                task();
            } catch (...) {
            }

            active_tasks_.fetch_sub(1);
        }
    }

private:
    std::string name_;
    std::size_t thread_count_;
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<IExecutorTaskQueue>> task_queues_;
    std::vector<std::thread> workers_;
    std::atomic<std::size_t> active_tasks_{0};
    std::atomic<std::size_t> next_queue_{0};
    bool running_{false};
    bool stopping_{false};
};

class SingleThreadExecutor : public ThreadPoolExecutor {
public:
    explicit SingleThreadExecutor(std::string name = "single")
        : ThreadPoolExecutor(std::move(name), 1) {}
};

class InferenceExecutor : public ThreadPoolExecutor {
public:
    explicit InferenceExecutor(std::string name = "inference", std::size_t thread_count = 1)
        : ThreadPoolExecutor(std::move(name), thread_count) {}
};

class IOExecutor : public ThreadPoolExecutor {
public:
    explicit IOExecutor(std::string name = "io", std::size_t thread_count = 1)
        : ThreadPoolExecutor(std::move(name), thread_count) {}
};

} // namespace common::thread
