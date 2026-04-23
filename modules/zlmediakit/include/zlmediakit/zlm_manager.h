#pragma once
#include <memory>
#include <atomic>
#include <thread>
#include <string>
#include <chrono>
#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <optional>
#include "common/config/common_config.h"
#include "zlmediakit/zlm_httpclient.h"
#include "zlmediakit/zlm_hookserver.h"

namespace Json = boost::json;

// ZLMediaKit 服务状态 
enum class ServiceStatus { 
    STATUS_STOPPED, 
    STATUS_STARTING, 
    STATUS_RUNNING, 
    STATUS_STOPPING, 
    STATUS_ERROR, 
};

/// @brief ZLM 进程管理（看门狗），负责启动、监控和停止 ZLMediaKit 进程
class ZLMProcessManager {
public:
    struct Config {
        std::string zlm_path;
        std::string config_path;
        std::string work_dir;
        bool debug_terminal{true}; // 是否开启调试终端
    };
    explicit ZLMProcessManager(boost::asio::io_context& ctx, const Config& cfg);
    ~ZLMProcessManager();
    static std::string GetZlmediakitPath();
    bool Start();
    void Stop();
    bool IsRunning() const;
    ServiceStatus GetStatus() const;
private:
    /// @brief 启动 ZLMediaKit 进程（调试模式: 开启终端窗口，在独立进程中运行）
    bool startZLMProcessDebug();
    /// @brief 启动 ZLMediaKit 进程（非调试模式: 运行在子进程中）
    bool startZLMProcessNormal();

    void onProcessExit(int exit_code, const std::error_code& ec);    
private:
    boost::asio::io_context& ctx_;
    Config config_;
    std::atomic<ServiceStatus> status_{ServiceStatus::STATUS_STOPPED};    
    /// @brief ZLMediaKit 进程对象,使用std::optional包装以支持延迟初始化
    std::optional<boost::process::process> zlm_process_;
};

class ZLMManager {
public:
   /// @brief 构造函数
   /// @param ctx io_context（用于进程管理和 API 调用）
   /// @param pool HTTP 连接池（用于 API 调用）
   /// @param zlm_config 媒体配置（包含 ZLM 地址、密钥等）
   explicit ZLMManager(boost::asio::io_context& ctx, 
                       Net::HttpClientPool* pool,
                       const ZlmConfig& zlm_config);
   ~ZLMManager();
   
   bool Start();
   void Stop();
   
   /// @brief 获取 ZLM 进程管理器
   ZLMProcessManager* getProcessManager() { return &process_; }
   
   /// @brief 获取 ZLM API 客户端
   ZLMApiClient* getApiClient() { return &api_client_; }

private:
    void RegisterRoutes();
    void handleZLMEvent(const std::string& event,
                       const Json::value& data);

private:
   boost::asio::io_context& ctx_;
   ZlmConfig zlm_config_;
   ZLMProcessManager process_;
   ZLMApiClient api_client_;
   std::unique_ptr<ZLMHookHandler> hook_handler_;  // 改为指针，避免构造问题
};






