#pragma once
#include <string>
#include <functional>
#include <boost/json.hpp>
#include <boost/asio.hpp>
#include "net/http_client/http_client_pool.h"
#include "zlmediakit/zlm_httpclient.h"
namespace Json = boost::json;

struct ZLMStreamPullerProxyInfo { 
    std::string key;     // 代理键（删除时必填）
    std::string vhost = "__defaultVhost__";
    std::string app;    // 应用名称
    std::string stream; // 流名称
    std::string url;    // 拉流地址
    int rtp_type; // rtp类型（tcp、udp、组播）
};
struct ZLMStreamPusherProxyInfo { 
    std::string key;     // 代理键（删除时必填）
    std::string vhost = "__defaultVhost__";
    std::string app;    // 应用名称
    std::string stream; // 流名称
    std::string url;    // 推流地址
    std::string schema; // 推流协议（rtmp、rtsp、rtmps, rtsps）
    int rtp_type; // rtp类型（tcp、udp、组播）
};


// ==================== 拉流代理管理类 ====================
/**
 * @brief ZLMediaKit 拉流代理管理模块
 * 
 * 功能：
 * - 添加/删除拉流代理
 * - 查询拉流代理信息
 */
class ZLMProxyManager {
public:    
    // 包朋友，允许 ZLMApiClient 访问
    friend class ZLMApiClient;
    explicit ZLMProxyManager(boost::asio::io_context& io_ctx, 
                                 Net::HttpClientPool* pool,
                                 const ZLMAddressConfig& cfg);

    // 拉流代理管理
    void AddStreamProxy(const ZLMStreamPullerProxyInfo& info);
    void DelStreamProxy(const ZLMStreamPullerProxyInfo& info);

    void GetProxyInfo(const ZLMStreamPullerProxyInfo& info);

    // 推流代理管理
    void AddStreamPusherProxy(const ZLMStreamPusherProxyInfo& info);
    void DelStreamPusherProxy(const ZLMStreamPusherProxyInfo& info);

	void GetMediaList(const boost::json::object& json_obj = {});
private:
    boost::asio::io_context& io_context_;
    Net::HttpClientPool* pool_;  // HTTP 连接池
    ZLMAddressConfig config_;
};
