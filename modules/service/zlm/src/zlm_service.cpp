#include "service/zlm/zlm_service.h"
#include "common/log/logmanager.h"
#include "net/http_client/http_client_pool.h"

std::shared_ptr<ZLMService> ZLMService::CreateFromAppConfig(const AppConfig& app_config) {
    return std::make_shared<ZLMService>(app_config.zlm);
}

ZLMService::ZLMService(const ZlmConfig& config)
    : config_(config) {
}

ZLMService::~ZLMService() {
    if (running_) {
        Stop();
    }
    
    // 确保线程被清理
    if (io_thread_ && io_thread_->joinable()) {
        io_thread_->join();
    }
}

bool ZLMService::Initialize() {
    if (initialized_) {
        LOG_MAIN_INFO_AT("{}: Already initialized", GetName());
        return true;
    }
    
    LOG_MAIN_INFO_AT("{}: Initializing...", GetName());
    
    try {
        // 创建 io_context
        io_context_ = std::make_unique<boost::asio::io_context>();
        
        // 检查 HttpClientPool 是否已设置
        if (!http_pool_) {
            LOG_MAIN_ERROR_AT("{}: HttpClientPool not set. Call SetHttpClientPool() before Initialize()", GetName());
            return false;
        }
        
        LOG_MAIN_INFO_AT("{}: HttpClientPool is ready", GetName());
        
        // 使用 new 创建 ZLMManager，并传入必要的参数
        zlm_manager_ = std::unique_ptr<ZLMManager>(new ZLMManager(*io_context_, http_pool_, config_));
        
        initialized_ = true;
        LOG_MAIN_INFO_AT("{}: Initialized successfully (host: {}, port: {}, secret: {})", 
                        GetName(), config_.zlm_host, config_.zlm_port, config_.secret);
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Initialization failed: {}", GetName(), e.what());
        return false;
    }
}

bool ZLMService::Start() {
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
        // 启动 ZLMManager
        if (zlm_manager_ && !zlm_manager_->Start()) {
            LOG_MAIN_ERROR_AT("{}: Failed to start ZLMManager", GetName());
            return false;
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

void ZLMService::Stop() {
    if (!running_) {
        LOG_MAIN_WARN_AT("{}: Not running", GetName());
        return;
    }
    
    LOG_MAIN_INFO_AT("{}: Stopping...", GetName());
    
    try {
        // 停止 ZLMManager
        if (zlm_manager_) {
            zlm_manager_->Stop();
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

