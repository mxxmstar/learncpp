#pragma once

#include <boost/json.hpp>
#include <string>

/// @brief 系统管理 API Handler
/// 处理所有 /system/* 路径的请求
class SystemApiHandler {
public:
    /// @brief 统一的模块路由处理器
    static void Handle(const std::string& path,
                      const boost::json::object& req,
                      boost::json::object& rsp);

private:
    // ==================== 系统状态 ====================
    /// @brief 获取系统状态
    /// GET /system/status
    static void handleGetStatus(const boost::json::object& req, boost::json::object& rsp);

    /// @brief 获取服务器配置
    /// GET /system/config
    static void handleGetConfig(const boost::json::object& req, boost::json::object& rsp);

    /// @brief 重启服务器
    /// POST /system/restart
    static void handleRestart(const boost::json::object& req, boost::json::object& rsp);

    // ==================== 工具方法 ====================
    /// @brief 检查必需参数是否存在
    static bool checkRequiredParams(const boost::json::object& req,
                                   const std::vector<std::string>& params,
                                   boost::json::object& rsp);
};
