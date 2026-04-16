#pragma once

#include "web/service/iservice.h"
#include "net/httpclientpool.h"
#include "config/common_config.h"
#include <boost/asio.hpp>
#include <memory>

/// @brief HTTP 客户端池服务
/// 封装 HttpClientPool，提供连接池管理服务
class HttpClientPoolService : public IService {
public:
    explicit HttpClientPoolService(boost::asio::io_context& ctx, const ClientPoolConfig& config);
    ~HttpClientPoolService() override;
    
    bool initialize() override;
    bool start() override;
    void stop() override;
    const char* getName() const override { return "HttpClientPoolService"; }
    bool isRunning() const override { return running_; }
    bool isInitialized() const override { return initialized_; }
    
    /// @brief 获取 HttpClientPool 实例
    Net::HttpClientPool* getHttpClientPool() { return pool_.get(); }
    
private:
    /// @brief 主 io_context，用于接收 HTTP 请求和处理响应
    boost::asio::io_context& ctx_;
    ClientPoolConfig config_;
    std::unique_ptr<Net::HttpClientPool> pool_ = nullptr;
    bool initialized_ = false;
    bool running_ = false;
};
