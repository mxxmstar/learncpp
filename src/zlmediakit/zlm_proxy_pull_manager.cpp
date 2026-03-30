#include "zlmediakit/zlm_proxy_pull_manager.h"
#include "zlmediakit/zlm_httpclient.h"

ZLMProxyPullManager::ZLMProxyPullManager(boost::asio::io_context& io_ctx, const ZLMAddressConfig& cfg)
    : io_context_(io_ctx), config_(cfg) 
{
}

void ZLMProxyPullManager::AddStreamProxy(const ZLMStreamProxyInfo& info) {
    boost::json::object params;    
    params["vhost"] = info.vhost;
    params["app"] = info.app;
    params["stream"] = info.stream;
    params["url"] = info.url;
    ZLMRequestHelper::DoRequest(io_context_, config_, "addStreamProxy", params);
}
void ZLMProxyPullManager::DelStreamProxy(const ZLMStreamProxyInfo& info) {
    boost::json::object params;
    params["key"] = info.key;
    ZLMRequestHelper::DoRequest(io_context_, config_, "delStreamProxy", params);
}
void ZLMProxyPullManager::GetProxyInfo(const ZLMStreamProxyInfo& info) {
    boost::json::object params;
    params["key"] = info.key;
    ZLMRequestHelper::DoRequest(io_context_, config_, "getProxyInfo", params);
}

