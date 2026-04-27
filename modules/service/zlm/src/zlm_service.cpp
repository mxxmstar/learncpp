#include "service/zlm/zlm_service.h"
#include "common/log/logmanager.h"
#include "net/http_client/http_client_pool.h"

std::shared_ptr<ZLMService> ZLMService::CreateFromAppConfig(const AppConfig& app_config) {
    return std::make_shared<ZLMService>(app_config.zlm);
}

ZLMService::ZLMService(const ZlmConfig& config)
    : config_(config), pool_(Net::AsioIOContextPool::GetInstance()) {
}

ZLMService::~ZLMService() {
    if (running_) {
        Stop();
    }
    // 线程池由全局单例管理，无需手动清理
}

bool ZLMService::Initialize() {
    if (initialized_) {
        LOG_MAIN_INFO_AT("{}: Already initialized", GetName());
        return true;
    }
    
    LOG_MAIN_INFO_AT("{}: Initializing...", GetName());
    
    try {
        // 注意：HttpClientPool 可以在 Initialize 之后、Start 之前设置
        // 这里不强制检查，允许延迟注入依赖
        if (http_pool_) {
            LOG_MAIN_INFO_AT("{}: HttpClientPool is ready", GetName());
        } else {
            LOG_MAIN_WARN_AT("{}: HttpClientPool not set yet, will check before Start()", GetName());
        }
        
        // 不在这里创建 ZLMManager，延迟到 Start() 时创建
        // 这样可以确保 http_pool_ 已经被注入
        
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
        // 在 Start 之前必须设置 HttpClientPool
        if (!http_pool_) {
            LOG_MAIN_ERROR_AT("{}: HttpClientPool not set. Call SetHttpClientPool() before Start()", GetName());
            return false;
        }
        
        // 延迟创建 ZLMManager，确保 http_pool_ 已经注入
        if (!zlm_manager_) {
            auto& io_ctx = pool_.GetOrCreateIOContext("zlm_manager");
            zlm_manager_ = std::unique_ptr<ZLMManager>(new ZLMManager(io_ctx, http_pool_, config_));
            LOG_MAIN_INFO_AT("{}: ZLMManager created with HttpClientPool", GetName());
        }
        
        // 启动 ZLMManager
        if (zlm_manager_ && !zlm_manager_->Start()) {
            LOG_MAIN_ERROR_AT("{}: Failed to start ZLMManager", GetName());
            return false;
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
        
        // 线程池由全局单例管理，无需手动停止
        
        running_ = false;
        LOG_MAIN_INFO_AT("{}: Stopped", GetName());
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Stop failed: {}", GetName(), e.what());
    }
}

