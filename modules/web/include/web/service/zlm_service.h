#pragma once

#include "web/service/iservice.h"
#include "zlmediakit/zlm_manager.h"
#include "config/common_config.h"
#include <boost/asio.hpp>
#include <memory>

/// @brief ZLMediaKit 服务
/// 封装 ZLMManager，提供流媒体服务
class ZLMService : public IService {
public:
    explicit ZLMService(boost::asio::io_context& ctx, const ZlmConfig& config);
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
    ZlmConfig config_;
    std::unique_ptr<ZLMManager> zlm_manager_;
    bool initialized_ = false;
    bool running_ = false;
};
