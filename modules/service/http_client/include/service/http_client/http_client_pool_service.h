#pragma once

#include "service/iservice.h"
#include "net/http_client/http_client_pool.h"
#include "net/io_context_pool/asio_io_context_pool.h"
#include "common/config/common_config.h"
#include <boost/asio.hpp>
#include <memory>

/// @brief HTTP 客户端池服务
/// 封装 HttpClientPool，提供连接池管理服务
class HttpClientPoolService : public IService {
public:
    /// @brief 从 HttpClientPoolConfig 创建
    explicit HttpClientPoolService(const HttpClientPoolConfig& config);
    
    /// @brief 从 AppConfig 创建（便捷方法）
    /// @param app_config 应用配置
    /// @return HttpClientPoolService 实例
    static std::shared_ptr<HttpClientPoolService> CreateFromAppConfig(const AppConfig& app_config);
    
    ~HttpClientPoolService() override;
    
    bool Initialize() override;
    bool Start() override;
    void Stop() override;
    const char* GetName() const override { return "HttpClientPoolService"; }
    bool IsRunning() const override { return running_; }
    bool IsInitialized() const override { return initialized_; }
    
    /// @brief 获取 HttpClientPool 实例
    Net::HttpClientPool* GetHttpClientPool() { return http_pool_.get(); }
    
private:
    HttpClientPoolConfig config_;
    Net::AsioIOContextPool& io_context_pool_;  // 引用全局线程池
    std::unique_ptr<Net::HttpClientPool> http_pool_;  // HTTP 客户端池
    bool initialized_ = false;
    bool running_ = false;
};
