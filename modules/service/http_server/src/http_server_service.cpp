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
    : config_(config) {
}

HttpServerService::~HttpServerService() {
    if (running_) {
        Stop();
    }
    
    // 确保线程被清理
    if (io_thread_ && io_thread_->joinable()) {
        io_thread_->join();
    }
}

bool HttpServerService::Initialize() {
    if (initialized_) {
        LOG_MAIN_INFO_AT("{}: Already initialized", GetName());
        return true;
    }
    
    LOG_MAIN_INFO_AT("{}: Initializing...", GetName());
    
    try {
        // 创建主 io_context
        io_context_ = std::make_unique<boost::asio::io_context>();
        
        // 创建 HTTP 服务器        
        // 假设 AsioHttpServer 的构造函数是 (io_context&, worker_pool, port)
        auto& worker_pool = AsioIOContextPool::GetInstance();
        server_ = std::make_unique<Net::AsioHttpServer>(*io_context_, worker_pool, config_.port);
        
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
