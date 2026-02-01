#pragma once
#include <memory>
#include <atomic>
#include <thread>
#include <string>
#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <optional>
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
   bool Start();
   void Stop();

private:
    void RegisterRoutes();
    void handleZLMEvent(const std::string& event,
                       const Json::value& data);

private:
   ZLMProcessManager process_;
   ZLMApiClient api_;
   ZLMHookHandler hook_;
};






