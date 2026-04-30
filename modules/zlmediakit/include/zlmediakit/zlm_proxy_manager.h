#pragma once
#include <string>
#include <functional>
#include <boost/json.hpp>
#include <boost/asio.hpp>
#include "net/http_client/http_client_pool.h"
#include "zlmediakit/zlm_httpclient.h"
namespace Json = boost::json;

struct ZLMStreamPullerProxyInfo {     
    std::string vhost_;  // 域名或 IP 地址
    std::string app_;    // 应用名称
    std::string stream_; // 流名称
    std::string url_;    // 拉流地址
    std::string key_;     // 代理键（删除时必填）
    // int rtp_type; // rtp类型（tcp、udp、组播）
    ZLMStreamPullerProxyInfo() : vhost_("__defaultVhost__") {}
    ZLMStreamPullerProxyInfo(std::string app, std::string stream, std::string url) 
        : vhost_("__defaultVhost__"), app_(std::move(app)), stream_(std::move(stream)), url_(std::move(url))
        , key_(vhost_ + "/" + app_ + "/" + stream_) {}
    ZLMStreamPullerProxyInfo(std::string key) : vhost_("__defaultVhost__"), key_(std::move(key)) {}
};
struct ZLMStreamPusherProxyInfo {     
    std::string vhost = "__defaultVhost__";
    std::string app;    // 应用名称
    std::string stream; // 流名称
    std::string url;    // 推流地址
    std::string schema; // 推流协议（rtmp、rtsp、rtmps, rtsps）
    std::string key;     // 代理键（删除时必填）
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
    void AddStreamProxy(const ZLMStreamPullerProxyInfo& info, ZLMRequestHelper::ResponseCallback callback = nullptr);
    void DelStreamProxy(const ZLMStreamPullerProxyInfo& info, ZLMRequestHelper::ResponseCallback callback = nullptr);

    void GetProxyInfo(const ZLMStreamPullerProxyInfo& info, ZLMRequestHelper::ResponseCallback callback = nullptr);

    // 推流代理管理
    void AddStreamPusherProxy(const ZLMStreamPusherProxyInfo& info, ZLMRequestHelper::ResponseCallback callback = nullptr);
    void DelStreamPusherProxy(const ZLMStreamPusherProxyInfo& info, ZLMRequestHelper::ResponseCallback callback = nullptr);

	void GetMediaList(const boost::json::object& json_obj, ZLMRequestHelper::ResponseCallback callback = nullptr);
private:
    boost::asio::io_context& io_context_;
    Net::HttpClientPool* pool_;  // HTTP 连接池
    ZLMAddressConfig config_;
};
