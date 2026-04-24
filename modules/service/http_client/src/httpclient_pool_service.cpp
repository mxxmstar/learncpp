#include "service/http_client/http_client_pool_service.h"
#include "common/log/logmanager.h"

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
    : config_(config), io_context_pool_(Net::AsioIOContextPool::GetInstance()) {
}

HttpClientPoolService::~HttpClientPoolService() {
    if (running_) {
        Stop();
    }
    // 线程池由全局单例管理，无需手动清理
}

bool HttpClientPoolService::Initialize() {
    if (initialized_) {
        LOG_MAIN_INFO_AT("{}: Already initialized", GetName());
        return true;
    }
    
    LOG_MAIN_INFO_AT("{}: Initializing...", GetName());
    
    try {
        // 创建并初始化 HttpClientPool（使用固定的 io_context）
        http_pool_ = std::make_unique<Net::HttpClientPool>();
        
        // 手动转换配置
        Net::HttpClientPool::Config pool_config;
        pool_config.host = config_.dst_host;
        pool_config.port = config_.dst_port;
        pool_config.init_size = config_.init_size;
        pool_config.max_size = config_.max_size;
        pool_config.connect_timeout_ms = config_.connect_timeout_ms;
        pool_config.idle_timeout_sec = config_.idle_timeout_sec;
        pool_config.max_requests_per_client = config_.max_requests_per_client;
        
        // 初始化连接池（使用固定的 io_context）
        auto& io_ctx = io_context_pool_.GetOrCreateIOContext("http_client_pool");
        http_pool_->Init(io_ctx, pool_config);
        
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
        // 线程池已经在 GetInstance() 时启动，无需额外操作
        
        running_ = true;
        LOG_MAIN_INFO_AT("{}: Started successfully (using thread pool)", GetName());
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
        if (http_pool_) {
            http_pool_->Stop();
        }
        
        // 线程池由全局单例管理，无需手动停止
        
        running_ = false;
        LOG_MAIN_INFO_AT("{}: Stopped", GetName());
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Stop failed: {}", GetName(), e.what());
    }
}
