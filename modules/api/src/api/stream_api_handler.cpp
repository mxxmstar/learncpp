#include "api/stream_api_handler.h"
#include "application/application.h"
#include "service/zlm/zlm_service.h"
#include "common/log/logmanager.h"
#include "zlmediakit/zlm_proxy_manager.h"
#include <boost/json.hpp>
#include <future>  // 用于异步等待
#include <chrono>  // 用于超时控制

namespace json = boost::json;
constexpr int kTimeoutSec = 5;

/**
 * @brief 获取并验证 ZLMService 实例
 * @param rsp 响应对象，失败时会设置错误码和消息
 * @return ZLMService 指针，失败返回 nullptr
 */
inline ZLMService* GetValidZLMService(boost::json::object& rsp) {
    auto& app = Application::GetInstance();
    auto zlm_svc = app.GetService<ZLMService>();
    if (!zlm_svc || !zlm_svc->IsInitialized()) {
        rsp["code"] = 503;
        rsp["msg"] = "ZLMService is not initialized";
        return nullptr;
    }
    return zlm_svc;
}

/**
 * @brief 从 ZLMService 获取 ZLMProxyManager
 * @param zlm_svc ZLMService 指针（必须非空）
 * @param rsp 响应对象，失败时会设置错误码和消息
 * @return ZLMProxyManager 指针，失败返回 nullptr
 */
inline ZLMProxyManager* GetZLMProxyManager(ZLMService* zlm_svc, boost::json::object& rsp) {
    auto* zlm_manager = zlm_svc->GetZLMManager();
    if (!zlm_manager) {
        rsp["code"] = 500;
        rsp["msg"] = "ZLMManager is null";
        return nullptr;
    }
    return &(zlm_manager->getApiClient()->Proxy());
}

/**
 * @brief 执行异步 ZLM API 调用并等待结果
 * @tparam Func 异步调用函数的类型
 * @param func 异步调用函数，接受一个回调函数作为参数
 * @param operation_name 操作名称，用于日志记录
 * @param rsp 响应对象，失败时会设置错误码和消息
 * @return pair<bool, json::object>，first 表示是否成功，second 是 ZLM 的响应
 */
template<typename Func>
std::pair<bool, boost::json::object> ExecuteZLMAsyncCall(
    Func&& func, 
    const std::string& operation_name,
    boost::json::object& rsp) 
{
    std::promise<std::pair<bool, boost::json::object>> promise;
    auto future = promise.get_future();
    
    func([&promise](bool success, const boost::json::object& response) {
        promise.set_value({success, response});
    });
    
    // 等待结果（最多等待 kTimeoutSec 秒）
    auto status = future.wait_for(std::chrono::seconds(kTimeoutSec));
    if (status == std::future_status::timeout) {
        LOG_MAIN_ERROR_AT("{} timeout", operation_name);
        rsp["code"] = 504;
        rsp["msg"] = "Request to ZLMediaKit timeout";
        return {false, {}};
    }
    
    auto [success, zlm_response] = future.get();
    
    if (!success) {
        LOG_MAIN_ERROR_AT("{} HTTP request failed", operation_name);
        rsp["code"] = 502;
        rsp["msg"] = "Failed to communicate with ZLMediaKit";
        return {false, {}};
    }
    
    // 检查 ZLMediaKit 返回的 code
    auto it = zlm_response.find("code");
    if (it != zlm_response.end() && it->value().is_int64()) {
        int zlm_code = static_cast<int>(it->value().as_int64());
        if (zlm_code != 0) {
            LOG_MAIN_WARN_AT("ZLMediaKit returned error code: {}", zlm_code);
            rsp["code"] = 500;
            rsp["msg"] = zlm_response.contains("msg") ? 
                boost::json::value_to<std::string>(zlm_response.at("msg")) : 
                "Unknown error";
            rsp["zlm_code"] = zlm_code;
            return {false, zlm_response};
        }
    }
    
    return {true, zlm_response};
}

void StreamApiHandler::Handle(const std::string& path,
                             const json::object& req,
                             json::object& rsp) {
    LOG_MAIN_INFO_AT("StreamApiHandler: path={}, req={}", path, json::serialize(req));

    try {
        // 路由分发到具体的处理函数
        if (path == "/addProxy") {
            handleAddStreamProxy(req, rsp);
        }
        else if (path == "/deleteProxy") {
            handleDeleteStreamProxy(req, rsp);
        }
        else if (path == "/proxyInfo") {
            handleGetProxyInfo(req, rsp);
        }
        else if (path == "/list") {
            handleGetMediaList(req, rsp);
        }
        else if (path == "/info") {
            handleGetMediaInfo(req, rsp);
        }
        else if (path == "/close") {
            handleCloseMedia(req, rsp);
        }
        else {
            rsp["code"] = 404;
            rsp["msg"] = "Unknown stream API path: " + path;
        }
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("StreamApiHandler exception: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Internal error: ") + e.what();
    }
}

void StreamApiHandler::handleAddStreamProxy(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Adding stream proxy..., req: {}", json::serialize(req));

    // 1. 检查必需参数
    if (!checkRequiredParams(req, {"app", "stream", "url"}, rsp)) {
        return;
    }

    try {
        // 2. 获取 ZLMService
        auto* zlm_svc = GetValidZLMService(rsp);
        if (!zlm_svc) {            
            return;
        }

        // 3. 解析参数
        // std::string vhost = json::value_to<std::string>(req.at("vhost"));
        std::string app_name = json::value_to<std::string>(req.at("app"));
        std::string stream = json::value_to<std::string>(req.at("stream"));
        std::string url = json::value_to<std::string>(req.at("url"));
        ZLMStreamPullerProxyInfo proxy_info(app_name, stream, url);
        // 可选参数设置

        // 4. 调用 ZLMClient 添加拉流代理                         
        auto* proxy = GetZLMProxyManager(zlm_svc, rsp);
        if (!proxy) {
            return;
        }        

        
        // 5.执行异步操作，等待结果（最多等待 kTimeoutSec 秒）
        auto [success, zlm_response] = ExecuteZLMAsyncCall(
            [&proxy, &proxy_info](auto&& callback) {
                proxy->AddStreamProxy(proxy_info, std::forward<decltype(callback)>(callback));
            },
            "AddStreamProxy",
            rsp
        );
        
        if (!success) {
            return;
        }
        
        // 成功
        rsp["code"] = 200;
        rsp["msg"] = "Success";
        rsp["data"] = {
            {"key", "__defaultVhost__/" + app_name + "/" + stream},
            {"app", app_name},
            {"stream", stream},
            {"url", url}
        };

        LOG_MAIN_INFO_AT("Stream proxy added: app={}, stream={}, url={}", app_name, stream, url);
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to add stream proxy: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to add proxy: ") + e.what();
    }
}

void StreamApiHandler::handleDeleteStreamProxy(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Deleting stream proxy..., req: {}", json::serialize(req));

    // 1. 检查必需参数
    bool param_with_key = true;
    bool param_with_app_stream = true;
    ZLMStreamPullerProxyInfo proxy_info{};
    if ((!req.contains("app")) || (!req.contains("stream"))) {
        param_with_app_stream = false;
    }

    if (!req.contains("key")) {
        param_with_key = false;
    }

    try {
        // 2. 获取 ZLMService
        auto* zlm_svc = GetValidZLMService(rsp);
        if (!zlm_svc) {            
            return;
        }

        // 3. 解析参数
        std::string app{}, stream{}, key{};
        if (param_with_key) {
            key = json::value_to<std::string>(req.at("key"));
            proxy_info = std::move(ZLMStreamPullerProxyInfo(key));
        } else if (param_with_app_stream) {
            app = json::value_to<std::string>(req.at("app"));
            stream = json::value_to<std::string>(req.at("stream"));
            proxy_info = std::move(ZLMStreamPullerProxyInfo(app, stream, ""));
        } else {
            rsp["code"] = 400;
            rsp["msg"] = "Missing required parameter: key or app/stream";
            return;
        }

        // 4. 调用 ZLMClient 添加拉流代理                         
        auto* proxy = GetZLMProxyManager(zlm_svc, rsp);
        if (!proxy) {
            return;
        }        

        
        // 5.执行异步操作，等待结果（最多等待 kTimeoutSec 秒）
        auto [success, zlm_response] = ExecuteZLMAsyncCall(
            [&proxy, &proxy_info](auto&& callback) {
                proxy->DelStreamProxy(proxy_info, std::forward<decltype(callback)>(callback));
            },
            "DeleteStreamProxy",
            rsp
        );
        
        if (!success) {
            return;
        }
        
        // 成功
        rsp["code"] = 200;
        rsp["msg"] = "Success";
        if (param_with_key) {
            rsp["data"] = {{"key", key}};
            LOG_MAIN_INFO_AT("Stream proxy deleted: key={}", key);
        } else {
            rsp["data"] = {{"app", app}, {"stream", stream}};
            LOG_MAIN_INFO_AT("Stream proxy deleted: app={}, stream={}", app, stream);
        }        
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to add stream proxy: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to add proxy: ") + e.what();
    }


    // // 检查必需参数
    // if (!checkRequiredParams(req, {"key"}, rsp)) {
    //     return;
    // }

    // try {
    //     std::string key = json::value_to<std::string>(req.at("key"));

    //     // TODO: 调用 ZLMManager 删除拉流代理
        
    //     rsp["code"] = 200;
    //     rsp["msg"] = "Success";
    //     rsp["data"] = {{"key", key}};

    //     LOG_MAIN_INFO_AT("Stream proxy deleted: key={}", key);
    // }
    // catch (const std::exception& e) {
    //     LOG_MAIN_ERROR_AT("Failed to delete stream proxy: {}", e.what());
    //     rsp["code"] = 500;
    //     rsp["msg"] = std::string("Failed to delete proxy: ") + e.what();
    // }
}

void StreamApiHandler::handleGetProxyInfo(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Getting proxy info...");

    // 检查必需参数
    if (!checkRequiredParams(req, {"key"}, rsp)) {
        return;
    }

    try {
        std::string key = json::value_to<std::string>(req.at("key"));

        // TODO: 调用 ZLMManager 查询代理信息
        
        rsp["code"] = 200;
        rsp["msg"] = "Success";
        rsp["data"] = {
            {"key", key},
            // TODO: 实际的代理信息
        };

        LOG_MAIN_INFO_AT("Proxy info retrieved: key={}", key);
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to get proxy info: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to get proxy info: ") + e.what();
    }
}

void StreamApiHandler::handleGetMediaList(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Getting media list...");

    try {
        // TODO: 调用 ZLMManager 获取媒体列表
        
        rsp["code"] = 200;
        rsp["msg"] = "Success";
        rsp["data"] = {
            {"count", 0},
            {"list", json::array{}}
        };

        LOG_MAIN_INFO_AT("Media list retrieved");
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to get media list: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to get media list: ") + e.what();
    }
}

void StreamApiHandler::handleGetMediaInfo(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Getting media info...");

    // 检查必需参数
    if (!checkRequiredParams(req, {"app", "stream"}, rsp)) {
        return;
    }

    try {
        std::string app = json::value_to<std::string>(req.at("app"));
        std::string stream = json::value_to<std::string>(req.at("stream"));

        // TODO: 调用 ZLMManager 获取媒体信息
        
        rsp["code"] = 200;
        rsp["msg"] = "Success";
        rsp["data"] = {
            {"app", app},
            {"stream", stream}
        };

        LOG_MAIN_INFO_AT("Media info retrieved: app={}, stream={}", app, stream);
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to get media info: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to get media info: ") + e.what();
    }
}

void StreamApiHandler::handleCloseMedia(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Closing media...");

    // 检查必需参数
    if (!checkRequiredParams(req, {"app", "stream"}, rsp)) {
        return;
    }

    try {
        std::string app = json::value_to<std::string>(req.at("app"));
        std::string stream = json::value_to<std::string>(req.at("stream"));

        // TODO: 调用 ZLMManager 关闭媒体流
        
        rsp["code"] = 200;
        rsp["msg"] = "Success";
        rsp["data"] = {
            {"app", app},
            {"stream", stream}
        };

        LOG_MAIN_INFO_AT("Media closed: app={}, stream={}", app, stream);
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to close media: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to close media: ") + e.what();
    }
}

bool StreamApiHandler::checkRequiredParams(const json::object& req,
                                          const std::vector<std::string>& params,
                                          json::object& rsp) {
    for (const auto& param : params) {
        if (req.find(param) == req.end()) {
            rsp["code"] = 400;
            rsp["msg"] = "Missing required parameter: " + param;
            return false;
        }
    }
    return true;
}



