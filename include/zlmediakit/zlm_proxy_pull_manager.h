#pragma once
#include <string>
#include <functional>
#include <boost/json.hpp>
#include <boost/asio.hpp>
#include "net/httpclientpool.h"
#include "zlmediakit/zlm_httpclient.h"
namespace Json = boost::json;
struct ZLMStreamProxyInfo { 
    std::string key;
    std::string vhost = "__defaultVhost__";
    std::string app;
    std::string stream;
    std::string url;
};

// ==================== 拉流代理管理类 ====================
/**
 * @brief ZLMediaKit 拉流代理管理模块
 * 
 * 功能：
 * - 添加/删除拉流代理
 * - 查询拉流代理信息
 */
class ZLMProxyPullManager {
public:
    using ResponseCallback = std::function<void(bool success, const Json::object& response)>;

    // 包朋友，允许 ZLMApiClient 访问
    friend class ZLMApiClient;
    explicit ZLMProxyPullManager(boost::asio::io_context& io_ctx, 
                                 Net::HttpClientPool* pool,
                                 const ZLMAddressConfig& cfg);

    // 拉流代理管理
    void AddStreamProxy(const ZLMStreamProxyInfo& info);
    void DelStreamProxy(const ZLMStreamProxyInfo& info);
    void GetProxyInfo(const ZLMStreamProxyInfo& info);
	void GetMediaList(const boost::json::object& json_obj = {});
private:
    boost::asio::io_context& io_context_;
    Net::HttpClientPool* pool_;  // HTTP 连接池
    ZLMAddressConfig config_;
};
