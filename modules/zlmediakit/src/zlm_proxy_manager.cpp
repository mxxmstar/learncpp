#include "zlmediakit/zlm_proxy_manager.h"
#include "zlmediakit/zlm_httpclient.h"
#include "net/http_client/http_client_pool.h"

ZLMProxyManager::ZLMProxyManager(boost::asio::io_context& io_ctx, 
                                         Net::HttpClientPool* pool,
                                         const ZLMAddressConfig& cfg)
    : io_context_(io_ctx), pool_(pool), config_(cfg) 
{
}

void ZLMProxyManager::AddStreamProxy(const ZLMStreamPullerProxyInfo& info, ZLMRequestHelper::ResponseCallback callback) {
    boost::json::object params;    
    params["vhost"] = info.vhost_;
    params["app"] = info.app_;
    params["stream"] = info.stream_;
    params["url"] = info.url_;
    ZLMRequestHelper::DoRequest(io_context_, pool_, config_, "addStreamProxy", params, callback);
}
void ZLMProxyManager::DelStreamProxy(const ZLMStreamPullerProxyInfo& info, ZLMRequestHelper::ResponseCallback callback) {
    boost::json::object params;
    params["key"] = info.key_;
    ZLMRequestHelper::DoRequest(io_context_, pool_, config_, "delStreamProxy", params, callback);
}
void ZLMProxyManager::GetProxyInfo(const ZLMStreamPullerProxyInfo& info, ZLMRequestHelper::ResponseCallback callback) {
    boost::json::object params;
    params["key"] = info.key_;
    ZLMRequestHelper::DoRequest(io_context_, pool_, config_, "getProxyInfo", params, callback);
}

void ZLMProxyManager::AddStreamPusherProxy(const ZLMStreamPusherProxyInfo& info, ZLMRequestHelper::ResponseCallback callback) {
    boost::json::object params;
    params["vhost"] = info.vhost;
    params["app"] = info.app;
    params["stream"] = info.stream;
    params["dst_url"] = info.url;
    params["schema"] = info.schema;    
    ZLMRequestHelper::DoRequest(io_context_, pool_, config_, "addStreamPusherProxy", params, callback);
}
void ZLMProxyManager::DelStreamPusherProxy(const ZLMStreamPusherProxyInfo& info, ZLMRequestHelper::ResponseCallback callback) {
    boost::json::object params;
    params["key"] = info.key;
    ZLMRequestHelper::DoRequest(io_context_, pool_, config_, "delStreamPusherProxy", params, callback);
}

void ZLMProxyManager::GetMediaList(const boost::json::object& json_obj, ZLMRequestHelper::ResponseCallback callback) {
    boost::json::object params = json_obj;
    ZLMRequestHelper::DoRequest(io_context_, pool_, config_, "getMediaList", params, callback);
}

