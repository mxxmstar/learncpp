#pragma once
#include <string>
#include <functional>
#include <memory>
#include <boost/json.hpp>
#include <boost/asio.hpp>
#include "net/httpclientpool.h"

// 前向声明 6 个功能管理类
//class ZLMStreamManager;
class ZLMProxyManager;
//class ZLMProxyPushManager;
//class ZLMRecordManager;
//class ZLMRtpManager;
//class ZLMSystemManager;

struct ZLMAddressConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 80;
    std::string secret;
};
// ==================== ZLM API 统一客户端 ====================
/**
 * @brief ZLMediaKit HTTP API 统一客户端
 * 
 * 组合了 5 个功能模块：
 * - StreamManager: 媒体流管理（查询、控制、会话管理）
 * - ProxyManager:流代理管理（推流代理、拉流代理）
 * - RecordManager: 录制管理（录制控制、文件管理、播放控制）
 * - RtpManager: RTP 服务管理（服务器、发送、连接）
 * - SystemManager: 系统管理（配置、状态、下载、截图等）
 */
class ZLMApiClient {
public:
    explicit ZLMApiClient(boost::asio::io_context& io_ctx, 
                          Net::HttpClientPool* pool,
                          const ZLMAddressConfig& cfg);
    ~ZLMApiClient();
    
    // 1. 媒体流管理
    //ZLMStreamManager& Stream();
    
    // 2. 推拉流代理管理
    ZLMProxyManager& Proxy();
        
    //// 4. 录制管理
    //ZLMRecordManager& Record();
    //
    //// 5. RTP 服务管理
    //ZLMRtpManager& Rtp();
    //
    //// 6. 系统管理
    //ZLMSystemManager& System();

private:
    boost::asio::io_context& io_context_;
    Net::HttpClientPool* pool_;  // HTTP 连接池
    ZLMAddressConfig config_;
    
    // 6 个功能模块（使用 unique_ptr 延迟初始化）
    //std::unique_ptr<ZLMStreamManager> stream_manager_;
    
    // 自定义删除器结构体（只需声明，实现在.cpp 中）
    struct ProxyDeleter {
        void operator()(ZLMProxyManager* ptr) const;
    };
    std::unique_ptr<ZLMProxyManager, ProxyDeleter> proxy_manager_;

    //std::unique_ptr<ZLMRecordManager> record_manager_;
    //std::unique_ptr<ZLMRtpManager> rtp_manager_;
    //std::unique_ptr<ZLMSystemManager> system_manager_;
};

class ZLMRequestHelper {
public:

    static void DoRequest(boost::asio::io_context& io_ctx,
        Net::HttpClientPool* pool,
        const ZLMAddressConfig& config,
        const std::string& api,
        const boost::json::object& params);
    static std::string BuildQuery(const boost::json::object& params);
};