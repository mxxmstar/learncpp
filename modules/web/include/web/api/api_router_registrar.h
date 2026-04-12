#pragma once

/// @brief API 路由注册器
/// 在系统启动时注册所有 HTTP API 路由
class ApiRouterRegistrar {
public:
    /// @brief 注册所有 API 路由
    static void RegisterAllRoutes();
};
