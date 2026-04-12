#include "web/service/httpclient_pool_service.h"
#include "log/logmanager.h"

namespace Net {
    // 前向声明 HttpClientPool
    class HttpClientPool;
}

HttpClientPoolService::HttpClientPoolService(boost::asio::io_context& ctx, const ClientPoolConfig& config)
    : ctx_(ctx), config_(config) {
}

HttpClientPoolService::~HttpClientPoolService() {
    if (running_) {
        stop();
    }
}

bool HttpClientPoolService::initialize() {
    if (initialized_) {
        LOG_MAIN_INFO_AT("{}: Already initialized", getName());
        return true;
    }
    
    LOG_MAIN_INFO_AT("{}: Initializing...", getName());
    
    try {
        // 创建并初始化 HttpClientPool（不再使用单例）
        pool_ = std::make_unique<Net::HttpClientPool>();
        
        // 直接使用 ClientPoolConfig 配置连接池
        Net::HttpClientPool::Config pool_config = config_.toHttpClientPoolConfig();
        
        // 初始化连接池
        pool_->Init(ctx_, pool_config);
        
        initialized_ = true;
        LOG_MAIN_INFO_AT("{}: Initialized successfully (host: {}, port: {}, init_size: {}, max_size: {})", 
                        getName(), config_.host, config_.port, config_.init_size, config_.max_size);
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Initialization failed: {}", getName(), e.what());
        return false;
    }
}

bool HttpClientPoolService::start() {
    if (!initialized_) {
        LOG_MAIN_ERROR_AT("{}: Not initialized", getName());
        return false;
    }
    
    if (running_) {
        LOG_MAIN_WARN_AT("{}: Already running", getName());
        return true;
    }
    
    LOG_MAIN_INFO_AT("{}: Starting...", getName());
    
    try {
        // HttpClientPool 在 Init 后就已经可以使用了，不需要额外的启动步骤
        running_ = true;
        LOG_MAIN_INFO_AT("{}: Started successfully", getName());
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Start failed: {}", getName(), e.what());
        return false;
    }
}

void HttpClientPoolService::stop() {
    if (!running_) {
        LOG_MAIN_WARN_AT("{}: Not running", getName());
        return;
    }
    
    LOG_MAIN_INFO_AT("{}: Stopping...", getName());
    
    try {
        // 停止连接池（如果 HttpClientPool 有 Stop 方法的话）
        if (pool_) {
            pool_->Stop();
        }
        
        running_ = false;
        LOG_MAIN_INFO_AT("{}: Stopped", getName());
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Stop failed: {}", getName(), e.what());
    }
}
