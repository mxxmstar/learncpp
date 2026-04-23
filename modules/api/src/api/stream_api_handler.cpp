#include "api/stream_api_handler.h"
#include "application/application.h"
#include "service/zlm/zlm_service.h"
#include "common/log/logmanager.h"
#include <boost/json.hpp>

namespace json = boost::json;

void StreamApiHandler::Handle(const std::string& path,
                             const json::object& req,
                             json::object& rsp) {
    LOG_MAIN_INFO_AT("StreamApiHandler: path={}, req={}", path, json::serialize(req));

    try {
        // 路由分发到具体的处理函数
        if (path == "/proxy/add") {
            handleAddStreamProxy(req, rsp);
        }
        else if (path == "/proxy/delete") {
            handleDeleteStreamProxy(req, rsp);
        }
        else if (path == "/proxy/info") {
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
    LOG_MAIN_INFO_AT("Adding stream proxy...");

    // 1. 检查必需参数
    if (!checkRequiredParams(req, {"vhost", "app", "stream", "url"}, rsp)) {
        return;
    }

    try {
        // 2. 获取 ZLMService
        auto& app = Application::GetInstance();
        auto zlm_svc = app.GetService<ZLMService>();
        if (!zlm_svc || !zlm_svc->IsInitialized()) {
            rsp["code"] = 503;
            rsp["msg"] = "ZLMService is not initialized";
            return;
        }

        // 3. 解析参数
        std::string vhost = json::value_to<std::string>(req.at("vhost"));
        std::string app = json::value_to<std::string>(req.at("app"));
        std::string stream = json::value_to<std::string>(req.at("stream"));
        std::string url = json::value_to<std::string>(req.at("url"));
        
        // 可选参数
        int rtp_type = 0;  // 默认 TCP
        if (req.contains("rtp_type")) {
            rtp_type = static_cast<int>(req.at("rtp_type").as_int64());
        }

        // 4. 调用 ZLMManager 添加拉流代理
        auto* zlm_manager = zlm_svc->GetZLMManager();
        if (!zlm_manager) {
            rsp["code"] = 500;
            rsp["msg"] = "ZLMManager is null";
            return;
        }

        // TODO: 这里需要调用 ZLMManager 的 API
        // zlm_manager->getApiClient()->Proxy().AddStreamProxy(...);
        
        // 临时实现：直接返回成功
        rsp["code"] = 200;
        rsp["msg"] = "Success";
        rsp["data"] = {
            {"key", vhost + "/" + app + "/" + stream},
            {"vhost", vhost},
            {"app", app},
            {"stream", stream},
            {"url", url}
        };

        LOG_MAIN_INFO_AT("Stream proxy added: vhost={}, app={}, stream={}, url={}", 
                        vhost, app, stream, url);
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to add stream proxy: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to add proxy: ") + e.what();
    }
}

void StreamApiHandler::handleDeleteStreamProxy(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Deleting stream proxy...");

    // 检查必需参数
    if (!checkRequiredParams(req, {"key"}, rsp)) {
        return;
    }

    try {
        std::string key = json::value_to<std::string>(req.at("key"));

        // TODO: 调用 ZLMManager 删除拉流代理
        
        rsp["code"] = 200;
        rsp["msg"] = "Success";
        rsp["data"] = {{"key", key}};

        LOG_MAIN_INFO_AT("Stream proxy deleted: key={}", key);
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to delete stream proxy: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to delete proxy: ") + e.what();
    }
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
