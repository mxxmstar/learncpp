#include "net/io_context_pool/asio_io_context_pool.h"
#include "common/log/logmanager.h"
#include <exception>
#include <thread>
namespace Net {
AsioIOContextPool& AsioIOContextPool::GetInstance(int size) {
    if (size == 0) size = std::thread::hardware_concurrency();
	static auto instance = AsioIOContextPool(size);
    return instance;
}

AsioIOContextPool::AsioIOContextPool(std::size_t size) 
    : high_concurrency_pool_(size)
    , high_work_guards_()
    , high_threads_()
    , next_high_io_context_(0)
    , is_running_(true)
{
    if (size == 0) size = 1;
    
    // 初始化高并发池
    for (size_t i = 0; i < size; ++i) {
        high_work_guards_.emplace_back(boost::asio::make_work_guard(high_concurrency_pool_[i]));
    }

    high_threads_.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        high_threads_.emplace_back([this, i]() {
            try {
                high_concurrency_pool_[i].run();
            }
            catch (const std::exception& e) {
                LOG_MAIN_ERROR_AT("High concurrency pool thread {} exception: {}", i, e.what());
            }
            catch (...) {
                LOG_MAIN_ERROR_AT("High concurrency pool thread {} unknown exception", i);  
            }
        });
    }
    
    LOG_MAIN_INFO_AT("AsioIOContextPool initialized: high_concurrency_pool_size={}", size);
}

AsioIOContextPool::~AsioIOContextPool() {
    Stop();
}

boost::asio::io_context& AsioIOContextPool::GetIOContext() {
    if (is_running_.load() == false) {
        throw std::runtime_error("AsioIOContextPool has been stopped");
    }
    // 轮询获取高并发池中的 io_context
    auto index = next_high_io_context_.fetch_add(1) % high_concurrency_pool_.size();
    return high_concurrency_pool_[index];
}

boost::asio::io_context& AsioIOContextPool::GetOrCreateIOContext(const std::string& group_name) {
    if (is_running_.load() == false) {
        throw std::runtime_error("AsioIOContextPool has been stopped");
    }
    
    std::lock_guard<std::mutex> lock(low_pool_mutex_);
    
    // 检查是否已经为该 group 创建过 io_context
    auto it = low_concurrency_pool_.find(group_name);
    if (it != low_concurrency_pool_.end()) {
        LOG_MAIN_DEBUG_AT("Group '{}' using existing low concurrency io_context", group_name);
        return *(it->second.io_context);
    }
    
    // 首次调用，创建新的 io_context 和线程
    auto io_ctx = std::make_unique<IOContext>();
    
    // 创建线程
    auto thread = std::make_unique<std::thread>([io_ctx_ptr = io_ctx.get()]() {
        try {
            io_ctx_ptr->run();
        }
        catch (const std::exception& e) {
            LOG_MAIN_ERROR_AT("Low concurrency pool thread for group exception: {}", e.what());
        }
        catch (...) {
            LOG_MAIN_ERROR_AT("Low concurrency pool thread for group unknown exception");
        }
    });
    
    // 存储（使用 emplace 避免赋值）
    auto& group = low_concurrency_pool_.emplace(group_name, LowConcurrencyGroup{}).first->second;
    group.io_context = std::move(io_ctx);
    group.work_guard.emplace(boost::asio::make_work_guard(*group.io_context));
    group.thread = std::move(thread);
    
    LOG_MAIN_INFO_AT("Group '{}' assigned new low concurrency io_context with dedicated thread", group_name);
    
    return *(low_concurrency_pool_[group_name].io_context);
}

void AsioIOContextPool::Stop() {
    if (is_running_.load() == false) {
        return; // 已经停止
    }
    is_running_.store(false);

    // 停止高并发池
    high_work_guards_.clear();
    for (auto& io_context : high_concurrency_pool_) {
        boost::asio::post(io_context, []() {});
    }
    for (auto& thread : high_threads_) {
        thread.join();
    }
    
    // 停止低并发池
    {
        std::lock_guard<std::mutex> lock(low_pool_mutex_);
        
        for (auto& [name, group] : low_concurrency_pool_) {
            if (group.io_context) {
                boost::asio::post(*group.io_context, []() {});
            }
        }
        
        for (auto& [name, group] : low_concurrency_pool_) {
            if (group.thread && group.thread->joinable()) {
                group.thread->join();
            }
        }
        
        low_concurrency_pool_.clear();
    }
    
    LOG_MAIN_INFO_AT("AsioIOContextPool stopped");
}
}