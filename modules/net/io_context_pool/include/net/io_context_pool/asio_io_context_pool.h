#pragma once
#include <boost/asio.hpp>
#include <vector>
#include <thread>
#include <string>
#include <map>
#include <mutex>
#include <optional>
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

    /// @brief 获取一个io_context 实例（轮询分配，用于高并发场景）
    /// @return io_context 实例
    boost::asio::io_context& GetIOContext();

    /// @brief 为 group 分配固定的 io_context（低并发场景）
    /// @param group_name 组名称（相同组的 Service 共享同一个 io_context）
    /// @return 固定的 io_context 引用（同一组始终返回相同）
    boost::asio::io_context& GetOrCreateIOContext(const std::string& group_name);

    /// @brief 停止线程池
    void Stop();

private:
    explicit AsioIOContextPool(std::size_t pool_size = std::thread::hardware_concurrency());

    // 高并发池：固定大小的 io_context 数组
    std::vector<IOContext> high_concurrency_pool_;
    std::vector<WorkGuard> high_work_guards_;
    std::vector<std::thread> high_threads_;
    std::atomic<std::size_t> next_high_io_context_{ 0 };
    
    // 低并发池：动态创建的 io_context（按 group_name）
    struct LowConcurrencyGroup {
        std::unique_ptr<IOContext> io_context;
        std::optional<WorkGuard> work_guard;  // 使用 optional 避免默认构造问题
        std::unique_ptr<std::thread> thread;
    };
    std::map<std::string, LowConcurrencyGroup> low_concurrency_pool_;
    std::mutex low_pool_mutex_;  // 保护低并发池
    
    std::atomic<bool> is_running_{ false };

};
}