#pragma once

#include <boost/json.hpp>
#include <string>

/// @brief 摄像头管理 API Handler
/// 处理所有 /camera/* 路径的请求
class CameraApiHandler {
public:
    /// @brief 统一的模块路由处理器
    static void Handle(const std::string& path,
                      const boost::json::object& req,
                      boost::json::object& rsp);

private:
    // ==================== CRUD 操作 ====================
    /// @brief 添加摄像头
    /// POST /camera/add
    static void handleAdd(const boost::json::object& req, boost::json::object& rsp);

    /// @brief 删除摄像头
    /// POST /camera/remove
    static void handleRemove(const boost::json::object& req, boost::json::object& rsp);

    /// @brief 更新摄像头信息
    /// POST /camera/update
    static void handleUpdate(const boost::json::object& req, boost::json::object& rsp);

    /// @brief 获取单个摄像头信息
    /// GET /camera/get
    static void handleGet(const boost::json::object& req, boost::json::object& rsp);

    /// @brief 获取所有摄像头列表
    /// GET /camera/list
    static void handleList(const boost::json::object& req, boost::json::object& rsp);

    // ==================== 查询操作 ====================
    /// @brief 按状态查询摄像头
    /// GET /camera/by_status
    static void handleGetByStatus(const boost::json::object& req, boost::json::object& rsp);

    /// @brief 按厂商查询摄像头
    /// GET /camera/by_vendor
    static void handleGetByVendor(const boost::json::object& req, boost::json::object& rsp);

    // ==================== 状态管理 ====================
    /// @brief 更新摄像头状态
    /// POST /camera/update_status
    static void handleUpdateStatus(const boost::json::object& req, boost::json::object& rsp);

    // ==================== 工具方法 ====================
    /// @brief 检查必需参数是否存在
    static bool checkRequiredParams(const boost::json::object& req,
                                   const std::vector<std::string>& params,
                                   boost::json::object& rsp);
};
