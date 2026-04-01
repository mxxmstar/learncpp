#pragma once
#include <boost/asio.hpp>
#include <vector>
#include <thread>
namespace Net {
/// @brief io_context线程池,启用多个线程，每个线程运行一个io_context实例
class AsioIOContextPool {
public:
    using IOContext = boost::asio::io_context;
    using WorkGuard = boost::asio::executor_work_guard<IOContext::executor_type>;
    /// @brief 获取单例 
    static AsioIOContextPool& GetInstance(int size = std::thread::hardware_concurrency());
    ~AsioIOContextPool();
    /// @brief 禁止拷贝和赋值
    AsioIOContextPool(const AsioIOContextPool&) = delete;
    AsioIOContextPool& operator=(const AsioIOContextPool&) = delete;

    /// @brief 获取一个io_context 实例
    /// @return io_context 实例
    boost::asio::io_context& GetIOContext();


    /// @brief 停止线程池
    void Stop();

private:
    explicit AsioIOContextPool(std::size_t pool_size = std::thread::hardware_concurrency());

    std::vector<IOContext> io_contexts_;
    std::vector<WorkGuard> work_guards_;
    std::vector<std::thread> threads_;
    std::atomic<std::size_t> next_io_context_{ 0 };
    std::atomic<bool> is_running_{ false };

};
}