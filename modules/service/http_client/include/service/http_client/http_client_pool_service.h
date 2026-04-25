#pragma once

#include "service/iservice.h"
#include "net/http_client/http_client_pool.h"
#include "net/io_context_pool/asio_io_context_pool.h"
#include "common/config/common_config.h"
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <map>
#include <string>

/// @brief HTTP 客户端池服务（支持多目标）
/// 封装多个 HttpClientPool，每个目标一个池
class HttpClientPoolService : public IService {
public:
    /// @brief 从 AppConfig 创建
    explicit HttpClientPoolService(const AppConfig& app_config);
    
    ~HttpClientPoolService() override;
    
    bool Initialize() override;
    bool Start() override;
    void Stop() override;
    const char* GetName() const override { return "HttpClientPoolService"; }
    bool IsRunning() const override { return running_; }
    bool IsInitialized() const override { return initialized_; }
    
    /// @brief 获取指定目标的 HttpClientPool 实例
    /// @param target_key 目标标识（host:port）
    /// @return HttpClientPool 指针，如果不存在返回 nullptr
    Net::HttpClientPool* GetClientPool(const std::string& target_key) const;
    
    /// @brief 获取 ZLM 客户端池（便捷方法）
    /// @return ZLM HttpClientPool 指针
    Net::HttpClientPool* GetZlmClientPool() const;
    
    /// @brief 获取所有 HttpClientPool 实例
    /// @return 目标标识到 HttpClientPool 的映射
    const std::map<std::string, std::unique_ptr<Net::HttpClientPool>>& GetAllPools() const { return http_pools_; }
    
private:
    /// @brief 生成目标标识
    static std::string makeTargetKey(const std::string& host, uint16_t port);
    
    /// @brief 从 AppConfig 提取 ZLM 客户端配置
    std::vector<HttpClientPoolConfig> extractZlmConfigs(const AppConfig& app_config);
    
    AppConfig app_config_;
    Net::AsioIOContextPool& io_context_pool_;  // 引用全局线程池
    std::map<std::string, std::unique_ptr<Net::HttpClientPool>> http_pools_;  // 多个 HTTP 客户端池
    bool initialized_ = false;
    bool running_ = false;
};
