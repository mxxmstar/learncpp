#pragma once

#include "common/service/iservice.h"
#include "zlmediakit/zlm_manager.h"
#include "config/common_config.h"
#include <boost/asio.hpp>
#include <memory>

// 前向声明，避免依赖 web 模块
namespace Net {
    class HttpClientPool;
}

namespace zlmediakit {

/// @brief ZLMediaKit 服务
/// 封装 ZLMManager，提供流媒体服务
class ZLMService : public IService {
public:
    /// @brief 构造函数
    /// @param ctx io_context
    /// @param http_pool HTTP 客户端池（由调用者提供）
    /// @param config ZLM 配置
    explicit ZLMService(boost::asio::io_context& ctx, 
                       Net::HttpClientPool* http_pool,
                       const ZlmConfig& config);
    ~ZLMService() override;
    
    bool initialize() override;
    bool start() override;
    void stop() override;
    const char* getName() const override { return "ZLMService"; }
    bool isRunning() const override { return running_; }
    bool isInitialized() const override { return initialized_; }
    
    /// @brief 获取 ZLMManager 指针
    ZLMManager* getZLMManager() { return zlm_manager_.get(); }
    
private:
    boost::asio::io_context& ctx_;
    Net::HttpClientPool* http_pool_;  // HTTP 客户端池（由外部提供）
    ZlmConfig config_;
    std::unique_ptr<ZLMManager> zlm_manager_;
    bool initialized_ = false;
    bool running_ = false;
};

} // namespace zlmediakit
