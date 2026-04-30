#pragma once

#include <boost/json.hpp>
#include <string>

class Application;

/// @brief 流管理 API Handler
/// 处理所有 /stream/* 路径的请求
class StreamApiHandler {
public:
    /// @brief 统一的模块路由处理器
    static void Handle(const std::string& path,
                      const boost::json::object& req,
                      boost::json::object& rsp);

private:
    // ==================== 拉流代理管理 ====================
    /// @brief 添加拉流代理
    /// POST /stream/proxy/add
    static void handleAddStreamProxy(const boost::json::object& req, boost::json::object& rsp);

    /// @brief 删除拉流代理
    /// DELETE /stream/proxy/delete
    static void handleDeleteStreamProxy(const boost::json::object& req, boost::json::object& rsp);

    /// @brief 查询拉流代理信息
    /// GET /stream/proxy/info
    static void handleGetProxyInfo(const boost::json::object& req, boost::json::object& rsp);

    // ==================== 媒体流管理 ====================
    /// @brief 获取媒体流列表
    /// GET /stream/list
    static void handleGetMediaList(const boost::json::object& req, boost::json::object& rsp);

    /// @brief 获取媒体流详细信息
    /// GET /stream/info
    static void handleGetMediaInfo(const boost::json::object& req, boost::json::object& rsp);

    /// @brief 关闭媒体流
    /// POST /stream/close
    static void handleCloseMedia(const boost::json::object& req, boost::json::object& rsp);

    // ==================== 工具方法 ====================
    /// @brief 检查必需参数是否存在
    static bool checkRequiredParams(const boost::json::object& req, 
                                   const std::vector<std::string>& params,
                                   boost::json::object& rsp);
                                 
};
