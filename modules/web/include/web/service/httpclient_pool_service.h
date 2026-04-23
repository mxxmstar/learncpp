#pragma once

#include "common/service/iservice.h"
#include "net/http_client/http_client_pool.h"
#include "config/common_config.h"
#include <boost/asio.hpp>
#include <memory>
#include <thread>

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
    Net::HttpClientPool* GetHttpClientPool() { return pool_.get(); }
    
private:
    HttpClientPoolConfig config_;
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;  // 工作守卫，防止 io_context 自动停止
    std::unique_ptr<std::thread> io_thread_;  // io_context 运行线程
    std::unique_ptr<Net::HttpClientPool> pool_ = nullptr;
    bool initialized_ = false;
    bool running_ = false;
};
