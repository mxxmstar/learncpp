#pragma once

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace common::thread::asio {

class AsioExecutor {
public:
    using Task = std::function<void()>;
    using IOContext = boost::asio::io_context;
    using WorkGuard = boost::asio::executor_work_guard<IOContext::executor_type>;

    explicit AsioExecutor(std::string name, std::size_t thread_count = 1)
        : name_(std::move(name))
        , thread_count_(thread_count == 0 ? 1 : thread_count) {}

    ~AsioExecutor() {
        Stop();
    }

    AsioExecutor(const AsioExecutor&) = delete;
    AsioExecutor& operator=(const AsioExecutor&) = delete;

    void Start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            return;
        }

        io_context_.restart();
        work_guard_.emplace(boost::asio::make_work_guard(io_context_));
        accepting_.store(true);
        running_ = true;

        workers_.reserve(thread_count_);
        for (std::size_t i = 0; i < thread_count_; ++i) {
            workers_.emplace_back([this]() {
                io_context_.run();
            });
        }
    }

    void Stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                return;
            }

            accepting_.store(false);
            work_guard_.reset();
        }

        boost::asio::post(io_context_, []() {});

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        workers_.clear();

        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }

    bool Post(Task task) {
        if (!accepting_.load()) {
            return false;
        }

        pending_.fetch_add(1);

        try {
            boost::asio::post(io_context_, [this, task = std::move(task)]() mutable {
                try {
                    task();
                } catch (...) {
                }
                pending_.fetch_sub(1);
            });
        } catch (...) {
            pending_.fetch_sub(1);
            return false;
        }

        return true;
    }

    IOContext& GetIOContext() {
        return io_context_;
    }

    const std::string& Name() const {
        return name_;
    }

    std::size_t Pending() const {
        return pending_.load();
    }

private:
    std::string name_;
    std::size_t thread_count_;
    IOContext io_context_;
    std::optional<WorkGuard> work_guard_;
    std::vector<std::thread> workers_;
    std::atomic_bool accepting_{false};
    std::atomic<std::size_t> pending_{0};
    std::mutex mutex_;
    bool running_{false};
};

class SingleThreadAsioExecutor : public AsioExecutor {
public:
    explicit SingleThreadAsioExecutor(std::string name = "single")
        : AsioExecutor(std::move(name), 1) {}
};

class CpuAsioExecutor : public AsioExecutor {
public:
    explicit CpuAsioExecutor(std::string name = "cpu",
                             std::size_t thread_count = std::thread::hardware_concurrency())
        : AsioExecutor(std::move(name), thread_count == 0 ? 1 : thread_count) {}
};

class InferenceAsioExecutor : public AsioExecutor {
public:
    explicit InferenceAsioExecutor(std::string name = "inference",
                                   std::size_t thread_count = 1)
        : AsioExecutor(std::move(name), thread_count) {}
};

class IOAsioExecutor : public AsioExecutor {
public:
    explicit IOAsioExecutor(std::string name = "io", std::size_t thread_count = 1)
        : AsioExecutor(std::move(name), thread_count) {}
};

} // namespace common::thread::asio
