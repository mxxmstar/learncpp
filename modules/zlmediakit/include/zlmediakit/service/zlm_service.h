#pragma once

#include "common/service/iservice.h"
#include "zlmediakit/zlm_manager.h"
#include "config/common_config.h"
#include <boost/asio.hpp>
#include <memory>
#include <thread>

// 前向声明，避免依赖 web 模块
namespace Net {
    class HttpClientPool;
}

/// @brief ZLMediaKit 服务
/// 封装 ZLMManager，提供流媒体服务
class ZLMService : public IService {
public:
    /// @brief 从 ZlmConfig 创建（HttpClientPool 通过 SetHttpClientPool 设置）
    /// @param config ZLM 配置
    explicit ZLMService(const ZlmConfig& config);
    
    /// @brief 从 AppConfig 创建（便捷方法）
    /// @param app_config 应用配置
    /// @return ZLMService 实例
    static std::shared_ptr<ZLMService> CreateFromAppConfig(const AppConfig& app_config);
    
    ~ZLMService() override;
    
    bool Initialize() override;
    bool Start() override;
    void Stop() override;
    const char* GetName() const override { return "ZLMService"; }
    bool IsRunning() const override { return running_; }
    bool IsInitialized() const override { return initialized_; }
    
    /// @brief 设置 HttpClientPool（在 Initialize 之前调用）
    void SetHttpClientPool(Net::HttpClientPool* pool) { http_pool_ = pool; }
    
    /// @brief 获取 ZLMManager 指针
    ZLMManager* GetZLMManager() { return zlm_manager_.get(); }
    
private:
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<std::thread> io_thread_;  // io_context 运行线程
    Net::HttpClientPool* http_pool_ = nullptr;  // HTTP 客户端池（外部设置）
    ZlmConfig config_;
    std::unique_ptr<ZLMManager> zlm_manager_;
    bool initialized_ = false;
    bool running_ = false;
};
