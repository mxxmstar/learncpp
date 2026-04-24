#include "service/http_server/http_server_service.h"
#include "common/log/logmanager.h"

using namespace Net; 
// 前向声明，确保 AsioHttpServer 的定义可见
class AsioHttpServer;

std::shared_ptr<HttpServerService> HttpServerService::CreateFromAppConfig(const AppConfig& app_config) {
    HttpServerConfig http_config;
    http_config.host = app_config.server.host;
    http_config.port = app_config.server.port;
    
    return std::make_shared<HttpServerService>(http_config);
}

HttpServerService::HttpServerService(const HttpServerConfig& config)
    : config_(config), pool_(Net::AsioIOContextPool::GetInstance()) {
}

HttpServerService::~HttpServerService() {
    if (running_) {
        Stop();
    }
    // 线程池由全局单例管理，无需手动清理
}

bool HttpServerService::Initialize() {
    if (initialized_) {
        LOG_MAIN_INFO_AT("{}: Already initialized", GetName());
        return true;
    }
    
    LOG_MAIN_INFO_AT("{}: Initializing...", GetName());
    
    try {
        // 创建 HTTP 服务器
        // acceptor 使用固定的 io_context（低并发）
        auto& accept_ioc = pool_.GetOrCreateIOContext("http_server_acceptor");
        
        // worker_pool 使用同一个线程池，但轮询分配（高并发）
        server_ = std::make_unique<Net::AsioHttpServer>(accept_ioc, pool_, config_.port);
        
        initialized_ = true;
        LOG_MAIN_INFO_AT("{}: Initialized successfully (host: {}, port: {})", 
                        GetName(), config_.host, config_.port);
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Initialization failed: {}", GetName(), e.what());
        return false;
    }
}

bool HttpServerService::Start() {
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
        // 启动 HTTP 服务器
        if (server_) {
            server_->Start();
        }
        
        // 线程池已经在 GetInstance() 时启动，无需额外操作
        
        running_ = true;
        LOG_MAIN_INFO_AT("{}: Started successfully (using thread pool)", GetName());
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Start failed: {}", GetName(), e.what());
        return false;
    }
}

void HttpServerService::Stop() {
    if (!running_) {
        LOG_MAIN_WARN_AT("{}: Not running", GetName());
        return;
    }
    
    LOG_MAIN_INFO_AT("{}: Stopping...", GetName());
    
    try {
        // 停止 HTTP 服务器
        if (server_) {
            server_->Stop();
        }
        
        // 线程池由全局单例管理，无需手动停止
        
        running_ = false;
        LOG_MAIN_INFO_AT("{}: Stopped", GetName());
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Stop failed: {}", GetName(), e.what());
    }
}
