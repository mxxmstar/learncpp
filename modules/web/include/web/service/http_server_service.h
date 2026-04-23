#pragma once

#include "common/service/iservice.h"
#include "net/http_server/http_server.h"
#include "net/io_context_pool/asio_io_context_pool.h"
#include "config/common_config.h"
#include <boost/asio.hpp>
#include <memory>
#include <thread>

/// @brief HTTP 服务器服务
/// 封装 AsioHttpServer，提供 HTTP 服务
class HttpServerService : public IService {
public:
    /// @brief 从 HttpServerConfig 创建
    explicit HttpServerService(const HttpServerConfig& config);
    
    /// @brief 从 AppConfig 创建（便捷方法）
    /// @param app_config 应用配置
    /// @return HttpServerService 实例
    static std::shared_ptr<HttpServerService> CreateFromAppConfig(const AppConfig& app_config);
    
    ~HttpServerService() override;
    
    bool Initialize() override;
    bool Start() override;
    void Stop() override;
    const char* GetName() const override { return "HttpServerService"; }
    bool IsRunning() const override { return running_; }
    bool IsInitialized() const override { return initialized_; }
    
    /// @brief 获取 io_context
    boost::asio::io_context* getIoContext() { return io_context_.get(); }
    
    /// @brief 获取 HTTP 服务器指针
    Net::AsioHttpServer* getHttpServer() { return server_.get(); }
    
private:
    HttpServerConfig config_;
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<std::thread> io_thread_;  // io_context 运行线程
    std::unique_ptr<Net::AsioHttpServer> server_;
    bool initialized_ = false;
    bool running_ = false;
};
