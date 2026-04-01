#include "service/http_server_service.h"
#include "log/logmanager.h"

using namespace Net; 
// 前向声明，确保 AsioHttpServer 的定义可见
class AsioHttpServer;


HttpServerService::HttpServerService(const ServerConfig& config)
    : config_(config) {
}

HttpServerService::~HttpServerService() {
    if (running_) {
        stop();
    }
}

bool HttpServerService::initialize() {
    if (initialized_) {
        LOG_MAIN_INFO_AT("{}: Already initialized", getName());
        return true;
    }
    
    LOG_MAIN_INFO_AT("{}: Initializing...", getName());
    
    try {
        // 创建 io_context
        io_context_ = std::make_unique<boost::asio::io_context>();
        
        // 创建 HTTP 服务器        
        // 假设 AsioHttpServer 的构造函数是 (io_context&, worker_pool, port)
        auto& worker_pool = AsioIOContextPool::GetInstance();
        server_ = std::make_unique<Net::AsioHttpServer>(*io_context_, worker_pool, config_.port);
        
        initialized_ = true;
        LOG_MAIN_INFO_AT("{}: Initialized successfully (host: {}, port: {})", 
                        getName(), config_.host, config_.port);
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Initialization failed: {}", getName(), e.what());
        return false;
    }
}

bool HttpServerService::start() {
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
        // 启动 HTTP 服务器
        if (server_) {
            server_->Start();
        }
        
        running_ = true;
        LOG_MAIN_INFO_AT("{}: Started successfully", getName());
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Start failed: {}", getName(), e.what());
        return false;
    }
}

void HttpServerService::stop() {
    if (!running_) {
        LOG_MAIN_WARN_AT("{}: Not running", getName());
        return;
    }
    
    LOG_MAIN_INFO_AT("{}: Stopping...", getName());
    
    try {
        // 停止 HTTP 服务器
        if (server_) {
            server_->Stop();
        }
        
        // 停止 io_context
        if (io_context_) {
            io_context_->stop();
        }
        
        running_ = false;
        LOG_MAIN_INFO_AT("{}: Stopped", getName());
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Stop failed: {}", getName(), e.what());
    }
}
