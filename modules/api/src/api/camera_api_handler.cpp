#include "api/camera_api_handler.h"
#include "camera/camera_storage.h"
#include "camera/camera.h"
#include "common/log/logmanager.h"
#include <boost/json.hpp>

namespace json = boost::json;

void CameraApiHandler::Handle(const std::string& path,
                             const json::object& req,
                             json::object& rsp) {
    LOG_MAIN_INFO_AT("CameraApiHandler: path={}, req={}", path, json::serialize(req));

    try {
        // 路由分发到具体的处理函数
        if (path == "/add") {
            handleAdd(req, rsp);
        }
        else if (path == "/remove") {
            handleRemove(req, rsp);
        }
        else if (path == "/update") {
            handleUpdate(req, rsp);
        }
        else if (path == "/get") {
            handleGet(req, rsp);
        }
        else if (path == "/list") {
            handleList(req, rsp);
        }
        else if (path == "/by_status") {
            handleGetByStatus(req, rsp);
        }
        else if (path == "/by_vendor") {
            handleGetByVendor(req, rsp);
        }
        else if (path == "/update_status") {
            handleUpdateStatus(req, rsp);
        }
        else {
            rsp["code"] = 404;
            rsp["msg"] = "Unknown camera API path: " + path;
        }
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CameraApiHandler exception: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Internal error: ") + e.what();
    }
}

void CameraApiHandler::handleAdd(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Adding camera...");

    try {
        // 检查必需参数
        if (!checkRequiredParams(req, {"uuid", "name", "rtsp_url"}, rsp)) {
            return;
        }

        // 从 JSON 构建 CameraInfo
        CameraInfo camera = CameraInfo::FromJsonObject(req);

        // 添加到数据库
        auto& storage = CameraStorage::GetInstance();
        bool success = storage.Add(camera);

        if (success) {
            rsp["code"] = 200;
            rsp["msg"] = "Camera added successfully";
            rsp["data"] = CameraInfo::ToJsonObject(camera);
            LOG_MAIN_INFO_AT("Camera added: uuid={}", camera.GetUuid());
        } else {
            rsp["code"] = 400;
            rsp["msg"] = "Failed to add camera (may already exist)";
            LOG_MAIN_WARN_AT("Failed to add camera: uuid={}", camera.GetUuid());
        }
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to add camera: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to add camera: ") + e.what();
    }
}

void CameraApiHandler::handleRemove(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Removing camera...");

    try {
        // 检查必需参数
        if (!checkRequiredParams(req, {"uuid"}, rsp)) {
            return;
        }

        std::string uuid = json::value_to<std::string>(req.at("uuid"));

        // 从数据库删除
        auto& storage = CameraStorage::GetInstance();
        bool success = storage.Remove(uuid);

        if (success) {
            rsp["code"] = 200;
            rsp["msg"] = "Camera removed successfully";
            LOG_MAIN_INFO_AT("Camera removed: uuid={}", uuid);
        } else {
            rsp["code"] = 404;
            rsp["msg"] = "Camera not found";
            LOG_MAIN_WARN_AT("Camera not found: uuid={}", uuid);
        }
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to remove camera: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to remove camera: ") + e.what();
    }
}

void CameraApiHandler::handleUpdate(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Updating camera...");

    try {
        // 检查必需参数
        if (!checkRequiredParams(req, {"uuid"}, rsp)) {
            return;
        }

        // 从 JSON 构建 CameraInfo
        CameraInfo camera = CameraInfo::FromJsonObject(req);

        // 更新数据库
        auto& storage = CameraStorage::GetInstance();
        bool success = storage.Update(camera);

        if (success) {
            rsp["code"] = 200;
            rsp["msg"] = "Camera updated successfully";
            rsp["data"] = CameraInfo::ToJsonObject(camera);
            LOG_MAIN_INFO_AT("Camera updated: uuid={}", camera.GetUuid());
        } else {
            rsp["code"] = 404;
            rsp["msg"] = "Camera not found";
            LOG_MAIN_WARN_AT("Failed to update camera: uuid={}", camera.GetUuid());
        }
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to update camera: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to update camera: ") + e.what();
    }
}

void CameraApiHandler::handleGet(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Getting camera info...");

    try {
        // 检查必需参数
        if (!checkRequiredParams(req, {"uuid"}, rsp)) {
            return;
        }

        std::string uuid = json::value_to<std::string>(req.at("uuid"));

        // 从数据库查询
        auto& storage = CameraStorage::GetInstance();
        CameraInfo camera;
        bool success = storage.Get(uuid, camera);

        if (success) {
            rsp["code"] = 200;
            rsp["msg"] = "Success";
            rsp["data"] = CameraInfo::ToJsonObject(camera);
            LOG_MAIN_INFO_AT("Camera retrieved: uuid={}", uuid);
        } else {
            rsp["code"] = 404;
            rsp["msg"] = "Camera not found";
            LOG_MAIN_WARN_AT("Camera not found: uuid={}", uuid);
        }
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to get camera: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to get camera: ") + e.what();
    }
}

void CameraApiHandler::handleList(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Getting camera list...");

    try {
        // 从数据库查询所有摄像头
        auto& storage = CameraStorage::GetInstance();
        std::vector<CameraInfo> cameras;
        bool success = storage.GetAll(cameras);

        if (success) {
            rsp["code"] = 200;
            rsp["msg"] = "Success";
            rsp["data"] = CameraInfo::ToJsonArray(cameras);
            rsp["total"] = static_cast<int>(cameras.size());
            LOG_MAIN_INFO_AT("Camera list retrieved: count={}", cameras.size());
        } else {
            rsp["code"] = 500;
            rsp["msg"] = "Failed to get camera list";
            LOG_MAIN_ERROR_AT("Failed to get camera list");
        }
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to get camera list: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to get camera list: ") + e.what();
    }
}

void CameraApiHandler::handleGetByStatus(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Getting cameras by status...");

    try {
        // 检查必需参数
        if (!checkRequiredParams(req, {"status"}, rsp)) {
            return;
        }

        std::string status_str = json::value_to<std::string>(req.at("status"));
        CameraStatus status = StringToCameraStatus(status_str);

        // 从数据库查询
        auto& storage = CameraStorage::GetInstance();
        std::vector<CameraInfo> cameras;
        bool success = storage.GetByStatus(status, cameras);

        if (success) {
            rsp["code"] = 200;
            rsp["msg"] = "Success";
            rsp["data"] = CameraInfo::ToJsonArray(cameras);
            rsp["total"] = static_cast<int>(cameras.size());
            rsp["status"] = status_str;
            LOG_MAIN_INFO_AT("Cameras by status retrieved: status={}, count={}", 
                           status_str, cameras.size());
        } else {
            rsp["code"] = 500;
            rsp["msg"] = "Failed to get cameras by status";
            LOG_MAIN_ERROR_AT("Failed to get cameras by status: {}", status_str);
        }
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to get cameras by status: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to get cameras by status: ") + e.what();
    }
}

void CameraApiHandler::handleGetByVendor(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Getting cameras by vendor...");

    try {
        // 检查必需参数
        if (!checkRequiredParams(req, {"vendor"}, rsp)) {
            return;
        }

        std::string vendor = json::value_to<std::string>(req.at("vendor"));

        // 从数据库查询
        auto& storage = CameraStorage::GetInstance();
        std::vector<CameraInfo> cameras;
        bool success = storage.GetByVendor(vendor, cameras);

        if (success) {
            rsp["code"] = 200;
            rsp["msg"] = "Success";
            rsp["data"] = CameraInfo::ToJsonArray(cameras);
            rsp["total"] = static_cast<int>(cameras.size());
            rsp["vendor"] = vendor;
            LOG_MAIN_INFO_AT("Cameras by vendor retrieved: vendor={}, count={}", 
                           vendor, cameras.size());
        } else {
            rsp["code"] = 500;
            rsp["msg"] = "Failed to get cameras by vendor";
            LOG_MAIN_ERROR_AT("Failed to get cameras by vendor: {}", vendor);
        }
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to get cameras by vendor: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to get cameras by vendor: ") + e.what();
    }
}

void CameraApiHandler::handleUpdateStatus(const json::object& req, json::object& rsp) {
    LOG_MAIN_INFO_AT("Updating camera status...");

    try {
        // 检查必需参数
        if (!checkRequiredParams(req, {"uuid", "status"}, rsp)) {
            return;
        }

        std::string uuid = json::value_to<std::string>(req.at("uuid"));
        std::string status_str = json::value_to<std::string>(req.at("status"));
        CameraStatus status = StringToCameraStatus(status_str);

        // 更新状态
        auto& storage = CameraStorage::GetInstance();
        bool success = storage.UpdateStatus(uuid, status);

        if (success) {
            rsp["code"] = 200;
            rsp["msg"] = "Camera status updated successfully";
            rsp["data"] = {
                {"uuid", uuid},
                {"status", status_str}
            };
            LOG_MAIN_INFO_AT("Camera status updated: uuid={}, status={}", uuid, status_str);
        } else {
            rsp["code"] = 404;
            rsp["msg"] = "Camera not found";
            LOG_MAIN_WARN_AT("Failed to update camera status: uuid={}", uuid);
        }
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to update camera status: {}", e.what());
        rsp["code"] = 500;
        rsp["msg"] = std::string("Failed to update camera status: ") + e.what();
    }
}

bool CameraApiHandler::checkRequiredParams(const json::object& req,
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
