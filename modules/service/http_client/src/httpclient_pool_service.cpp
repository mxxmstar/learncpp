#include "service/http_client/http_client_pool_service.h"
#include "common/log/logmanager.h"
#include <sstream>

namespace Net {
    // 前向声明 HttpClientPool
    class HttpClientPool;
}

std::string HttpClientPoolService::makeTargetKey(const std::string& host, uint16_t port) {
    return host + ":" + std::to_string(port);
}

HttpClientPoolService::HttpClientPoolService(const AppConfig& app_config)
    : app_config_(app_config), io_context_pool_(Net::AsioIOContextPool::GetInstance()) {
}

HttpClientPoolService::~HttpClientPoolService() {
    if (running_) {
        Stop();
    }
    // 线程池由全局单例管理，无需手动清理
}

std::vector<HttpClientPoolConfig> HttpClientPoolService::extractZlmConfigs(const AppConfig& app_config) {
    auto it = app_config.clients.find("zlm");
    if (it != app_config.clients.end() && !it->second.empty()) {
        return it->second;
    }
    
    // 如果没有配置，返回默认配置
    LOG_MAIN_WARN_AT("HttpClientPoolService: No ZLM client configs found, using default");
    HttpClientPoolConfig default_config;
    default_config.dst_host = "127.0.0.1";
    default_config.dst_port = 8080;
    default_config.init_size = 5;
    default_config.max_size = 20;
    default_config.connect_timeout_ms = 5000;
    default_config.idle_timeout_sec = 300;
    default_config.max_requests_per_client = 1000;
    
    return {default_config};
}

bool HttpClientPoolService::Initialize() {
    if (initialized_) {
        LOG_MAIN_INFO_AT("{}: Already initialized", GetName());
        return true;
    }
    
    // 提取 ZLM 配置
    auto configs = extractZlmConfigs(app_config_);
    
    LOG_MAIN_INFO_AT("{}: Initializing {} pools...", GetName(), configs.size());
    
    try {
        // 为每个配置创建 HttpClientPool
        for (size_t i = 0; i < configs.size(); ++i) {
            const auto& config = configs[i];
            std::string target_key = makeTargetKey(config.dst_host, config.dst_port);
            
            // 检查是否已存在
            if (http_pools_.find(target_key) != http_pools_.end()) {
                LOG_MAIN_WARN_AT("{}: Pool for {} already exists, skipping", GetName(), target_key);
                continue;
            }
            
            // 创建池
            auto pool = std::make_unique<Net::HttpClientPool>();
            
            // 转换配置
            Net::HttpClientPool::Config pool_config;
            pool_config.host = config.dst_host;
            pool_config.port = config.dst_port;
            pool_config.init_size = config.init_size;
            pool_config.max_size = config.max_size;
            pool_config.connect_timeout_ms = config.connect_timeout_ms;
            pool_config.idle_timeout_sec = config.idle_timeout_sec;
            pool_config.max_requests_per_client = config.max_requests_per_client;
            
            // 初始化连接池（使用固定的 io_context，按组共享）
            // std::string group_name = "http_client_" + target_key;
            // auto& io_ctx = io_context_pool_.GetOrCreateIOContext(group_name);
            auto& io_ctx = io_context_pool_.GetOrCreateIOContext("http_client");
            pool->Init(io_ctx, pool_config);
            
            http_pools_[target_key] = std::move(pool);
            
            LOG_MAIN_INFO_AT("{}: Pool [{}] initialized (host: {}, port: {}, init_size: {}, max_size: {})", 
                            GetName(), target_key, config.dst_host, config.dst_port, 
                            config.init_size, config.max_size);
        }
        
        initialized_ = true;
        LOG_MAIN_INFO_AT("{}: Initialized successfully with {} pools", GetName(), http_pools_.size());
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
    
    LOG_MAIN_INFO_AT("{}: Stopping {} pools...", GetName(), http_pools_.size());
    
    try {
        // 停止所有连接池
        for (auto& [key, pool] : http_pools_) {
            if (pool) {
                pool->Stop();
                LOG_MAIN_INFO_AT("{}: Pool [{}] stopped", GetName(), key);
            }
        }
        
        // 线程池由全局单例管理，无需手动停止
        
        running_ = false;
        LOG_MAIN_INFO_AT("{}: Stopped", GetName());
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("{}: Stop failed: {}", GetName(), e.what());
    }
}

Net::HttpClientPool* HttpClientPoolService::GetClientPool(const std::string& target_key) const {
    auto it = http_pools_.find(target_key);
    if (it != http_pools_.end()) {
        return it->second.get();
    }
    return nullptr;
}

Net::HttpClientPool* HttpClientPoolService::GetZlmClientPool() const {
    // 获取第一个 ZLM 配置的池
    auto it = app_config_.clients.find("zlm");
    if (it != app_config_.clients.end() && !it->second.empty()) {
        const auto& config = it->second[0];
        std::string target_key = makeTargetKey(config.dst_host, config.dst_port);
        return GetClientPool(target_key);
    }
    return nullptr;
}
