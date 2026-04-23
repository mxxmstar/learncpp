#include "zlmediakit/zlm_httpclient.h"
#include "zlmediakit/zlm_proxy_manager.h"
#include "net/http_client/http_client_pool.h"
#include "log/logmanager.h"
#include <sstream>
using namespace Net;

void ZLMRequestHelper::DoRequest(boost::asio::io_context& io_ctx, Net::HttpClientPool* pool,
    const ZLMAddressConfig& config, const std::string& api, const boost::json::object& params)
{
    // 使用传入的连接池（不再使用单例）
    if (!pool) {
        LOG_MAIN_ERROR_AT("ZLMApiClient: HttpClientPool is null");
        return;
    }

    // 使用 RAII 守卫获取客户端（自动管理生命周期）
    auto client_guard = pool->AcquireGuard();
    if (!client_guard) {
        LOG_MAIN_ERROR_AT("ZLMApiClient: failed to acquire HTTP client");        
        return;
    }

    // 步骤 3: 添加认证参数（secret）
    boost::json::object full_params = params;
    full_params["secret"] = config.secret;  // 添加密钥

    // 构建查询字符串
    std::string query = BuildQuery(full_params);

    // 步骤 5: 构建完整 URL
    std::string url = "/index/api/" + api + "?" + query;
    // 发送 HTTP GET 请求，直接传递空 handler
    client_guard->GetJsonWithHandler(url, [](bool success, const boost::json::object& rsp_obj) {
        /*for (auto& [key, value] : rsp_obj) {
			LOG_MAIN_DEBUG_AT("key: {}, value: {}", key, value);
        }*/
    });
}

std::string ZLMRequestHelper::BuildQuery(const boost::json::object& params) {
    std::ostringstream oss;
    bool first = true;

    for (const auto& [key, value] : params) {
        if (!first) oss << "&";
        first = false;

        oss << key << "=";
        
        // 根据 JSON 值类型转换为字符串
        if (value.is_string()) {
            oss << std::string(value.as_string());
        } else if (value.is_int64()) {
            oss << value.as_int64();
        } else if (value.is_double()) {
            oss << value.as_double();
        } else if (value.is_bool()) {
            oss << (value.as_bool() ? "1" : "0");
        }
    }

    return oss.str();  // 返回："app=live&stream=camera1&secret=your_secret"
}

ZLMApiClient::ZLMApiClient(boost::asio::io_context& io_ctx, 
                           Net::HttpClientPool* pool,
                           const ZLMAddressConfig& cfg)
    : io_context_(io_ctx), pool_(pool), config_(cfg) 
{
    if (!pool_) {
        throw std::runtime_error("ZLMApiClient requires a valid HttpClientPool");
    }
    // 初始化流代理管理器
    proxy_manager_.reset(new ZLMProxyManager(io_context_, pool_, config_));
}

ZLMApiClient::~ZLMApiClient() {
}

void ZLMApiClient::ProxyDeleter::operator()(ZLMProxyManager* ptr) const {
    delete ptr;
}

ZLMProxyManager& ZLMApiClient::Proxy() {
	return *proxy_manager_;
}
