#include "zlmediakit/zlm_proxy_manager.h"
#include "zlmediakit/zlm_httpclient.h"
#include "net/http_client/http_client_pool.h"

ZLMProxyManager::ZLMProxyManager(boost::asio::io_context& io_ctx, 
                                         Net::HttpClientPool* pool,
                                         const ZLMAddressConfig& cfg)
    : io_context_(io_ctx), pool_(pool), config_(cfg) 
{
}

void ZLMProxyManager::AddStreamProxy(const ZLMStreamPullerProxyInfo& info) {
    boost::json::object params;    
    params["vhost"] = info.vhost;
    params["app"] = info.app;
    params["stream"] = info.stream;
    params["url"] = info.url;
    ZLMRequestHelper::DoRequest(io_context_, pool_, config_, "addStreamProxy", params);
}
void ZLMProxyManager::DelStreamProxy(const ZLMStreamPullerProxyInfo& info) {
    boost::json::object params;
    params["key"] = info.key;
    ZLMRequestHelper::DoRequest(io_context_, pool_, config_, "delStreamProxy", params);
}
void ZLMProxyManager::GetProxyInfo(const ZLMStreamPullerProxyInfo& info) {
    boost::json::object params;
    params["key"] = info.key;
    ZLMRequestHelper::DoRequest(io_context_, pool_, config_, "getProxyInfo", params);
}

void ZLMProxyManager::AddStreamPusherProxy(const ZLMStreamPusherProxyInfo& info) {
    boost::json::object params;
    params["vhost"] = info.vhost;
    params["app"] = info.app;
    params["stream"] = info.stream;
    params["dst_url"] = info.url;
    params["schema"] = info.schema;
    params["rtp_type"] = info.rtp_type;
    ZLMRequestHelper::DoRequest(io_context_, pool_, config_, "addStreamPusherProxy", params);
}
void ZLMProxyManager::DelStreamPusherProxy(const ZLMStreamPusherProxyInfo& info) {
    boost::json::object params;
    params["key"] = info.key;
    ZLMRequestHelper::DoRequest(io_context_, pool_, config_, "delStreamPusherProxy", params);
}

void ZLMProxyManager::GetMediaList(const boost::json::object& json_obj) {
    boost::json::object params = json_obj;
    ZLMRequestHelper::DoRequest(io_context_, pool_, config_, "getMediaList", params);
}

