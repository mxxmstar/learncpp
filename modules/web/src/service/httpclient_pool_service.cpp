#include "web/service/httpclient_pool_service.h"
#include "log/logmanager.h"

namespace Net {
    // 前向声明 HttpClientPool
    class HttpClientPool;
}

std::shared_ptr<HttpClientPoolService> HttpClientPoolService::CreateFromAppConfig(const AppConfig& app_config) {
    HttpClientPoolConfig pool_config;
    pool_config.dst_host = app_config.zlm_client.dst_host;
    pool_config.dst_port = app_config.zlm_client.dst_port;
    pool_config.init_size = app_config.zlm_client.init_size;
    pool_config.max_size = app_config.zlm_client.max_size;
    pool_config.connect_timeout_ms = app_config.zlm_client.connect_timeout_ms;
    pool_config.idle_timeout_sec = app_config.zlm_client.idle_timeout_sec;
    pool_config.max_requests_per_client = app_config.zlm_client.max_requests_per_client;
    
    return std::make_shared<HttpClientPoolService>(pool_config);
}

HttpClientPoolService::HttpClientPoolService(const HttpClientPoolConfig& config)
    : config_(config) {
}

HttpClientPoolService::~HttpClientPoolService() {
    if (running_) {
        Stop();
    }
    
    // 确保线程被清理
    if (io_thread_ && io_thread_->joinable()) {
        io_thread_->join();
    }
}

bool HttpClientPoolService::Initialize() {
    if (initialized_) {
        LOG_MAIN_INFO_AT("{}: Already initialized", GetName());
        return true;
    }
    
    LOG_MAIN_INFO_AT("{}: Initializing...", GetName());
    
    try {
        // 创建 io_context
        io_context_ = std::make_unique<boost::asio::io_context>();
        
        // 创建并初始化 HttpClientPool（不再使用单例）
        pool_ = std::make_unique<Net::HttpClientPool>();
        
        // 手动转换配置
        Net::HttpClientPool::Config pool_config;
        pool_config.host = config_.dst_host;
        pool_config.port = config_.dst_port;
        pool_config.init_size = config_.init_size;
        pool_config.max_size = config_.max_size;
        pool_config.connect_timeout_ms = config_.connect_timeout_ms;
        pool_config.idle_timeout_sec = config_.idle_timeout_sec;
        pool_config.max_requests_per_client = config_.max_requests_per_client;
        
        // 初始化连接池
        pool_->Init(*io_context_, pool_config);
        
        initialized_ = true;
        LOG_MAIN_INFO_AT("{}: Initialized successfully (host: {}, port: {}, init_size: {}, max_size: {})", 
                        GetName(), config_.dst_host, config_.dst_port, config_.init_size, config_.max_size);
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Initialization failed: {}", GetName(), e.what());
        return false;
    }
}

bool HttpClientPoolService::Start() {
    if (!initialized_) {
        LOG_MAIN_ERROR_AT("{}: Not initialized", GetName());
        return false;
    }
    
    if (running_) {
        LOG_MAIN_WARN_AT("{}: Already running", GetName());
        return true;
    }
    
    LOG_MAIN_INFO_AT("{}: Starting...", GetName());
    
    try {
        // 创建工作守卫，防止 io_context 在没有任务时自动停止
        work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            io_context_->get_executor()
        );
        
        // 启动 io_context 运行线程
        io_thread_ = std::make_unique<std::thread>([this]() {
            LOG_MAIN_INFO_AT("{}: io_context running...", GetName());
            io_context_->run();
            LOG_MAIN_INFO_AT("{}: io_context stopped", GetName());
        });
        
        running_ = true;
        LOG_MAIN_INFO_AT("{}: Started successfully", GetName());
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Start failed: {}", GetName(), e.what());
        return false;
    }
}

void HttpClientPoolService::Stop() {
    if (!running_) {
        LOG_MAIN_WARN_AT("{}: Not running", GetName());
        return;
    }
    
    LOG_MAIN_INFO_AT("{}: Stopping...", GetName());
    
    try {
        // 停止连接池
        if (pool_) {
            pool_->Stop();
        }
        
        // 重置工作守卫，允许 io_context 停止
        if (work_guard_) {
            work_guard_.reset();
            LOG_MAIN_INFO_AT("{}: Work guard reset", GetName());
        }
        
        // 停止 io_context
        if (io_context_) {
            io_context_->stop();
        }
        
        // 等待 io_context 线程结束
        if (io_thread_ && io_thread_->joinable()) {
            LOG_MAIN_INFO_AT("{}: Waiting for io_context thread to finish...", GetName());
            io_thread_->join();
            LOG_MAIN_INFO_AT("{}: io_context thread joined", GetName());
        }
        
        running_ = false;
        LOG_MAIN_INFO_AT("{}: Stopped", GetName());
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Stop failed: {}", GetName(), e.what());
    }
}
