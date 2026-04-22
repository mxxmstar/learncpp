#include "zlmediakit/service/zlm_service.h"
#include "log/logmanager.h"
#include "net/httpclientpool.h"  // 直接依赖 net 模块，而不是 web 模块

namespace zlmediakit {

ZLMService::ZLMService(boost::asio::io_context& ctx, 
                      Net::HttpClientPool* http_pool,
                      const ZlmConfig& config)
    : ctx_(ctx), http_pool_(http_pool), config_(config) {
}

ZLMService::~ZLMService() {
    if (running_) {
        Stop();
    }
}

bool ZLMService::Initialize() {
    if (initialized_) {
        LOG_MAIN_INFO_AT("{}: Already initialized", GetName());
        return true;
    }
    
    LOG_MAIN_INFO_AT("{}: Initializing...", GetName());
    
    try {
        // 检查 HTTP 客户端池是否有效
        if (!http_pool_) {
            LOG_MAIN_ERROR_AT("{}: HttpClientPool is null", GetName());
            return false;
        }
        
        LOG_MAIN_INFO_AT("{}: HttpClientPool is ready", GetName());
        
        // 使用 new 创建 ZLMManager，并传入必要的参数
        zlm_manager_ = std::unique_ptr<ZLMManager>(new ZLMManager(ctx_, http_pool_, config_));
        
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
        
        running_ = false;
        LOG_MAIN_INFO_AT("{}: Stopped", GetName());
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Stop failed: {}", GetName(), e.what());
    }
}

} // namespace zlmediakit
