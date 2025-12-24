#include "net/asio_io_context_pool.h"
#include <exception>
AsioIOContextPool::AsioIOContextPool(std::size_t size) : io_contexts_(size)
    , work_guards_(), threads_(), next_io_context_(0), is_running_(false) 
{
    if (size == 0) size == 1;
    for (size_t i = 0; i < size; ++i){
        work_guards_.emplace_back(boost::asio::make_work_guard(io_contexts_[i]));
    }

    threads_.reserve(size);
    for (std::size_t i = 0; i < size; ++i){
        threads_.emplace_back([this, i]() {
            try {
                // 运行io_context   
                io_contexts_[i].run();
            } catch (const std::exception& e) {
                // Handle exceptions as needed
            } catch (...) {
                
            }
        });
    }
}

AsioIOContextPool::~AsioIOContextPool() {
    Stop();
}

boost::asio::io_context& AsioIOContextPool::GetIOContext() {
    if (is_running_.load() == true) {
        throw std::runtime_error("AsioIOContextPool has been stopped");
    }
    // 轮询获取io_context
    auto index = next_io_context_.fetch_add(1) % io_contexts_.size();
    return io_contexts_[index];
}

void AsioIOContextPool::Stop() {
    if (is_running_.load() == false) {
        return; // 已经停止
    }
    is_running_.store(false);

    // 停止所有 work guard,允许 run() 在任务完成后退出
    work_guards_.clear();
    // 向所有 io_context 发送一个空任务，确保在 epoll/kqueue 的 run() 被唤醒停止
    for (auto& io_context : io_contexts_) {
        boost::asio::post(io_context, []() {});
        // io_context.stop();
    }
    for (auto& thread : threads_) {
        thread.join();
    }
}
