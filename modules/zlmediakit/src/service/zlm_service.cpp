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
        stop();
    }
}

bool ZLMService::initialize() {
    if (initialized_) {
        LOG_MAIN_INFO_AT("{}: Already initialized", getName());
        return true;
    }
    
    LOG_MAIN_INFO_AT("{}: Initializing...", getName());
    
    try {
        // 检查 HTTP 客户端池是否有效
        if (!http_pool_) {
            LOG_MAIN_ERROR_AT("{}: HttpClientPool is null", getName());
            return false;
        }
        
        LOG_MAIN_INFO_AT("{}: HttpClientPool is ready", getName());
        
        // 使用 new 创建 ZLMManager，并传入必要的参数
        zlm_manager_ = std::unique_ptr<ZLMManager>(new ZLMManager(ctx_, http_pool_, config_));
        
        initialized_ = true;
        LOG_MAIN_INFO_AT("{}: Initialized successfully (host: {}, port: {}, secret: {})", 
                        getName(), config_.zlm_host, config_.zlm_port, config_.secret);
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Initialization failed: {}", getName(), e.what());
        return false;
    }
}

bool ZLMService::start() {
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
        // 启动 ZLMManager
        if (zlm_manager_ && !zlm_manager_->Start()) {
            LOG_MAIN_ERROR_AT("{}: Failed to start ZLMManager", getName());
            return false;
        }
        
        running_ = true;
        LOG_MAIN_INFO_AT("{}: Started successfully", getName());
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Start failed: {}", getName(), e.what());
        return false;
    }
}

void ZLMService::stop() {
    if (!running_) {
        LOG_MAIN_WARN_AT("{}: Not running", getName());
        return;
    }
    
    LOG_MAIN_INFO_AT("{}: Stopping...", getName());
    
    try {
        // 停止 ZLMManager
        if (zlm_manager_) {
            zlm_manager_->Stop();
        }
        
        running_ = false;
        LOG_MAIN_INFO_AT("{}: Stopped", getName());
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Stop failed: {}", getName(), e.what());
    }
}

} // namespace zlmediakit
