#include "api/system_api_handler.h"
#include "service/service_container.h"
#include "service/zlm_service.h"
#include "log/logmanager.h"
#include <boost/json.hpp>

namespace json = boost::json;

void SystemApiHandler::Handle(const std::string& path,
                             const json::object& req,
                             json::object& rsp) {
    LOG_MAIN_INFO_AT("SystemApiHandler: path={}, req={}", path, json::serialize(req));

    try {
        // 路由分发到具体的处理函数
        if (path == "/status") {
            handleGetStatus(req, rsp);
        }
        else if (path == "/config") {
            handleGetConfig(req, rsp);
        }
        else if (path == "/restart") {
            handleRestart(req, rsp);
        }
        else {
            rsp["code"] = 404;
            rsp["msg"] = "Unknown system API path: " + path;
        }
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("SystemApiHandler exception: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Internal error: ") + e.what();
    }
}

void SystemApiHandler::handleGetStatus(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Getting system status...");

    try {
        // 获取 ZLMService 状态
        auto zlm_svc = ServiceContainer::getInstance().getService<ZLMService>();
        bool zlm_running = zlm_svc && zlm_svc->isRunning();

        // 构建响应
        rsp["code"] = 200;
        rsp["msg"] = "Success";
        rsp["data"] = {
            {"zlm_status", zlm_running ? "running" : "stopped"},
            {"timestamp", std::to_string(std::time(nullptr))}
        };

        LOG_MAIN_INFO_AT("System status retrieved: ZLM={}", zlm_running ? "running" : "stopped");
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to get system status: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to get status: ") + e.what();
    }
}

void SystemApiHandler::handleGetConfig(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Getting system config...");

    try {
        // TODO: 从 ConfigManager 获取配置
        
        rsp["code"] = 200;
        rsp["msg"] = "Success";
        rsp["data"] = {
            // TODO: 实际的配置信息
        };

        LOG_MAIN_INFO_AT("System config retrieved");
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to get system config: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to get config: ") + e.what();
    }
}

void SystemApiHandler::handleRestart(const json::object& req, json::object& rsp) {
    LOG_MAIN_WARN_AT("System restart requested!");

    try {
        // TODO: 实现系统重启逻辑
        
        rsp["code"] = 200;
        rsp["msg"] = "Restart scheduled";
        
        LOG_MAIN_WARN_AT("System will restart...");
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to restart system: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to restart: ") + e.what();
    }
}

bool SystemApiHandler::checkRequiredParams(const json::object& req,
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
