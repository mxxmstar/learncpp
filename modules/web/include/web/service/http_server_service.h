#pragma once

#include "common/service/iservice.h"
#include "net/httpserver.h"
#include "net/asio_io_context_pool.h"
#include "config/common_config.h"
#include <boost/asio.hpp>
#include <memory>

/// @brief HTTP 服务器服务
/// 封装 AsioHttpServer，提供 HTTP 服务
class HttpServerService : public IService {
public:
    explicit HttpServerService(const ServerConfig& config);
    ~HttpServerService() override;
    
    bool initialize() override;
    bool start() override;
    void stop() override;
    const char* getName() const override { return "HttpServerService"; }
    bool isRunning() const override { return running_; }
    bool isInitialized() const override { return initialized_; }
    
    /// @brief 获取 io_context
    boost::asio::io_context* getIoContext() { return io_context_.get(); }
    
    /// @brief 获取 HTTP 服务器指针
    Net::AsioHttpServer* getHttpServer() { return server_.get(); }
    
private:
    ServerConfig config_;
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<Net::AsioHttpServer> server_;
    bool initialized_ = false;
    bool running_ = false;
};
