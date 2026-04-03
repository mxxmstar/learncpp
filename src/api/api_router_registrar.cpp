#include "api/api_router_registrar.h"
#include "net/httprouter.h"
#include "api/stream_api_handler.h"
#include "api/system_api_handler.h"
#include "log/logmanager.h"

void ApiRouterRegistrar::RegisterAllRoutes() {
    LOG_MAIN_INFO_AT("Registering all API routes...");

    auto& router = Net::HttpRouter::GetInstance();

    // 注册流管理模块（所有 /stream/* 路径）
    router.RegisterModuleRoute("stream", StreamApiHandler::Handle);
    LOG_MAIN_INFO_AT("Registered module route: /stream/*");

    // 注册系统管理模块（所有 /system/* 路径）
    router.RegisterModuleRoute("system", SystemApiHandler::Handle);
    LOG_MAIN_INFO_AT("Registered module route: /system/*");

    // 注册一些简单的测试路由
    router.RegisterRoute("/api/ping", [](const boost::json::object& req, boost::json::object& rsp) {
        rsp["code"] = 200;
        rsp["msg"] = "pong";
        rsp["timestamp"] = std::to_string(std::time(nullptr));
    });
    LOG_MAIN_INFO_AT("Registered route: /api/ping");

    LOG_MAIN_INFO_AT("All API routes registered successfully!");
}
