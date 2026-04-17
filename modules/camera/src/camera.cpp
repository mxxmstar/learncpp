#include "camera/camera.h"
#include "camera/time_utils.h"
#include <sstream>
#include <iomanip>
#include <ctime>

// ==================== CameraBaseInfo ====================

boost::json::object CameraBaseInfo::ToJsonObject(const CameraBaseInfo& info) {
    boost::json::object obj;
    obj["uuid"] = info.uuid;
    obj["name"] = info.name;
    obj["vendor"] = info.vendor;
    obj["hardware"] = info.hardware;
    obj["software"] = info.software;
    obj["serialnumber"] = info.serialnumber;
    obj["customer"] = info.customer;
    obj["metadata"] = info.metadata;
    obj["create_time"] = info.create_time;  // int64_t
    obj["update_time"] = info.update_time;  // int64_t
    return obj;
}

CameraBaseInfo CameraBaseInfo::FromJsonObject(const boost::json::object& obj) {
    CameraBaseInfo info;
    if (obj.contains("uuid")) info.uuid = boost::json::value_to<std::string>(obj.at("uuid"));
    if (obj.contains("name")) info.name = boost::json::value_to<std::string>(obj.at("name"));
    if (obj.contains("vendor")) info.vendor = boost::json::value_to<std::string>(obj.at("vendor"));
    if (obj.contains("hardware")) info.hardware = boost::json::value_to<std::string>(obj.at("hardware"));
    if (obj.contains("software")) info.software = boost::json::value_to<std::string>(obj.at("software"));
    if (obj.contains("serialnumber")) info.serialnumber = boost::json::value_to<std::string>(obj.at("serialnumber"));
    if (obj.contains("customer")) info.customer = boost::json::value_to<std::string>(obj.at("customer"));
    if (obj.contains("metadata")) info.metadata = boost::json::value_to<std::string>(obj.at("metadata"));
    
    // 兼容 string 和 int64 两种格式
    if (obj.contains("create_time")) {
        if (obj.at("create_time").is_string()) {
            info.create_time = StringToTimestamp(boost::json::value_to<std::string>(obj.at("create_time")));
        } else {
            info.create_time = static_cast<int64_t>(obj.at("create_time").as_int64());
        }
    }
    if (obj.contains("update_time")) {
        if (obj.at("update_time").is_string()) {
            info.update_time = StringToTimestamp(boost::json::value_to<std::string>(obj.at("update_time")));
        } else {
            info.update_time = static_cast<int64_t>(obj.at("update_time").as_int64());
        }
    }
    return info;
}

// ==================== CameraConnectionInfo ====================

boost::json::object CameraConnectionInfo::ToJsonObject(const CameraConnectionInfo& info) {
    boost::json::object obj;
    obj["uuid"] = info.uuid;
    obj["rtsp_url"] = info.rtsp_url;
    obj["username"] = info.username;
    obj["password"] = info.password;  // 前期明文返回
    return obj;
}

CameraConnectionInfo CameraConnectionInfo::FromJsonObject(const boost::json::object& obj) {
    CameraConnectionInfo info;
    if (obj.contains("uuid")) info.uuid = boost::json::value_to<std::string>(obj.at("uuid"));
    if (obj.contains("rtsp_url")) info.rtsp_url = boost::json::value_to<std::string>(obj.at("rtsp_url"));
    if (obj.contains("username")) info.username = boost::json::value_to<std::string>(obj.at("username"));
    if (obj.contains("password")) info.password = boost::json::value_to<std::string>(obj.at("password"));
    return info;
}

// ==================== CameraProtocolInfo ====================

boost::json::object CameraProtocolInfo::ToJsonObject(const CameraProtocolInfo& info) {
    boost::json::object obj;
    obj["uuid"] = info.uuid;
    obj["protocol_type"] = info.protocol_type;
    obj["http_base_url"] = info.http_base_url;
    obj["onvif_device_url"] = info.onvif_device_url;
    obj["gb28181_id"] = info.gb28181_id;
    return obj;
}

CameraProtocolInfo CameraProtocolInfo::FromJsonObject(const boost::json::object& obj) {
    CameraProtocolInfo info;
    if (obj.contains("uuid")) info.uuid = boost::json::value_to<std::string>(obj.at("uuid"));
    if (obj.contains("protocol_type")) info.protocol_type = boost::json::value_to<std::string>(obj.at("protocol_type"));
    if (obj.contains("http_base_url")) info.http_base_url = boost::json::value_to<std::string>(obj.at("http_base_url"));
    if (obj.contains("onvif_device_url")) info.onvif_device_url = boost::json::value_to<std::string>(obj.at("onvif_device_url"));
    if (obj.contains("gb28181_id")) info.gb28181_id = boost::json::value_to<std::string>(obj.at("gb28181_id"));
    return info;
}

// ==================== CameraVideoParams ====================

boost::json::object CameraVideoParams::ToJsonObject(const CameraVideoParams& params) {
    boost::json::object obj;
    obj["uuid"] = params.uuid;
    obj["width"] = params.width;
    obj["height"] = params.height;
    obj["fps"] = params.fps;
    obj["bitrate"] = params.bitrate;
    return obj;
}

CameraVideoParams CameraVideoParams::FromJsonObject(const boost::json::object& obj) {
    CameraVideoParams params;
    if (obj.contains("uuid")) params.uuid = boost::json::value_to<std::string>(obj.at("uuid"));
    if (obj.contains("width")) params.width = static_cast<int>(obj.at("width").as_int64());
    if (obj.contains("height")) params.height = static_cast<int>(obj.at("height").as_int64());
    if (obj.contains("fps")) params.fps = static_cast<int>(obj.at("fps").as_int64());
    if (obj.contains("bitrate")) params.bitrate = static_cast<int>(obj.at("bitrate").as_int64());
    return params;
}

// ==================== CameraStatusInfo ====================

boost::json::object CameraStatusInfo::ToJsonObject(const CameraStatusInfo& info) {
    boost::json::object obj;
    obj["uuid"] = info.uuid;
    obj["status"] = CameraStatusToString(info.status);
    obj["last_online_time"] = info.last_online_time;  // int64_t
    obj["offline_count"] = info.offline_count;
    obj["update_time"] = info.update_time;  // int64_t
    return obj;
}

CameraStatusInfo CameraStatusInfo::FromJsonObject(const boost::json::object& obj) {
    CameraStatusInfo info;
    if (obj.contains("uuid")) info.uuid = boost::json::value_to<std::string>(obj.at("uuid"));
    if (obj.contains("status")) {
        std::string status_str = boost::json::value_to<std::string>(obj.at("status"));
        info.status = StringToCameraStatus(status_str);
    }
    
    // 兼容 string 和 int64 两种格式
    if (obj.contains("last_online_time")) {
        if (obj.at("last_online_time").is_string()) {
            info.last_online_time = StringToTimestamp(boost::json::value_to<std::string>(obj.at("last_online_time")));
        } else {
            info.last_online_time = static_cast<int64_t>(obj.at("last_online_time").as_int64());
        }
    }
    if (obj.contains("offline_count")) info.offline_count = static_cast<int>(obj.at("offline_count").as_int64());
    if (obj.contains("update_time")) {
        if (obj.at("update_time").is_string()) {
            info.update_time = StringToTimestamp(boost::json::value_to<std::string>(obj.at("update_time")));
        } else {
            info.update_time = static_cast<int64_t>(obj.at("update_time").as_int64());
        }
    }
    return info;
}

// ==================== CameraInfo ====================

boost::json::object CameraInfo::ToJsonObject(const CameraInfo& camera) {
    boost::json::object obj;
    
    // 嵌套结构
    obj["base"] = CameraBaseInfo::ToJsonObject(camera.base);
    obj["connection"] = CameraConnectionInfo::ToJsonObject(camera.connection);
    obj["protocol"] = CameraProtocolInfo::ToJsonObject(camera.protocol);
    obj["video_params"] = CameraVideoParams::ToJsonObject(camera.video_params);
    obj["status_info"] = CameraStatusInfo::ToJsonObject(camera.status_info);
    
    // 扁平化字段（方便前端使用）
    obj["uuid"] = camera.base.uuid;
    obj["name"] = camera.base.name;
    obj["vendor"] = camera.base.vendor;
    obj["rtsp_url"] = camera.connection.rtsp_url;
    obj["username"] = camera.connection.username;
    obj["password"] = camera.connection.password;  // 前期明文返回
    obj["protocol_type"] = camera.protocol.protocol_type;
    obj["width"] = camera.video_params.width;
    obj["height"] = camera.video_params.height;
    obj["fps"] = camera.video_params.fps;
    obj["bitrate"] = camera.video_params.bitrate;
    obj["status"] = CameraStatusToString(camera.status_info.status);
    obj["create_time"] = camera.base.create_time;  // int64_t
    obj["update_time"] = camera.base.update_time;  // int64_t
    obj["last_online_time"] = camera.status_info.last_online_time;  // int64_t
    
    return obj;
}

boost::json::array CameraInfo::ToJsonArray(const std::vector<CameraInfo>& cameras) {
    boost::json::array arr;
    for (const auto& camera : cameras) {
        arr.push_back(ToJsonObject(camera));
    }
    return arr;
}

CameraInfo CameraInfo::FromJsonObject(const boost::json::object& obj) {
    CameraInfo camera;
    
    // 尝试解析嵌套结构
    if (obj.contains("base")) {
        camera.base = CameraBaseInfo::FromJsonObject(obj.at("base").as_object());
    } else {
        // 兼容扁平化格式
        if (obj.contains("uuid")) camera.base.uuid = boost::json::value_to<std::string>(obj.at("uuid"));
        if (obj.contains("name")) camera.base.name = boost::json::value_to<std::string>(obj.at("name"));
        if (obj.contains("vendor")) camera.base.vendor = boost::json::value_to<std::string>(obj.at("vendor"));
        if (obj.contains("hardware")) camera.base.hardware = boost::json::value_to<std::string>(obj.at("hardware"));
        if (obj.contains("software")) camera.base.software = boost::json::value_to<std::string>(obj.at("software"));
        if (obj.contains("serialnumber")) camera.base.serialnumber = boost::json::value_to<std::string>(obj.at("serialnumber"));
        if (obj.contains("customer")) camera.base.customer = boost::json::value_to<std::string>(obj.at("customer"));
        if (obj.contains("metadata")) camera.base.metadata = boost::json::value_to<std::string>(obj.at("metadata"));
        
        // 兼容时间格式
        if (obj.contains("create_time")) {
            if (obj.at("create_time").is_string()) {
                camera.base.create_time = StringToTimestamp(boost::json::value_to<std::string>(obj.at("create_time")));
            } else {
                camera.base.create_time = static_cast<int64_t>(obj.at("create_time").as_int64());
            }
        }
        if (obj.contains("update_time")) {
            if (obj.at("update_time").is_string()) {
                camera.base.update_time = StringToTimestamp(boost::json::value_to<std::string>(obj.at("update_time")));
            } else {
                camera.base.update_time = static_cast<int64_t>(obj.at("update_time").as_int64());
            }
        }
    }
    
    if (obj.contains("connection")) {
        camera.connection = CameraConnectionInfo::FromJsonObject(obj.at("connection").as_object());
    } else {
        camera.connection.uuid = camera.base.uuid;
        if (obj.contains("rtsp_url")) camera.connection.rtsp_url = boost::json::value_to<std::string>(obj.at("rtsp_url"));
        if (obj.contains("username")) camera.connection.username = boost::json::value_to<std::string>(obj.at("username"));
        if (obj.contains("password")) camera.connection.password = boost::json::value_to<std::string>(obj.at("password"));
    }
    
    if (obj.contains("protocol")) {
        camera.protocol = CameraProtocolInfo::FromJsonObject(obj.at("protocol").as_object());
    } else {
        camera.protocol.uuid = camera.base.uuid;
        if (obj.contains("protocol_type")) camera.protocol.protocol_type = boost::json::value_to<std::string>(obj.at("protocol_type"));
        if (obj.contains("http_base_url")) camera.protocol.http_base_url = boost::json::value_to<std::string>(obj.at("http_base_url"));
        if (obj.contains("onvif_device_url")) camera.protocol.onvif_device_url = boost::json::value_to<std::string>(obj.at("onvif_device_url"));
        if (obj.contains("gb28181_id")) camera.protocol.gb28181_id = boost::json::value_to<std::string>(obj.at("gb28181_id"));
    }
    
    if (obj.contains("video_params")) {
        camera.video_params = CameraVideoParams::FromJsonObject(obj.at("video_params").as_object());
    } else {
        camera.video_params.uuid = camera.base.uuid;
        if (obj.contains("width")) camera.video_params.width = static_cast<int>(obj.at("width").as_int64());
        if (obj.contains("height")) camera.video_params.height = static_cast<int>(obj.at("height").as_int64());
        if (obj.contains("fps")) camera.video_params.fps = static_cast<int>(obj.at("fps").as_int64());
        if (obj.contains("bitrate")) camera.video_params.bitrate = static_cast<int>(obj.at("bitrate").as_int64());
    }
    
    if (obj.contains("status_info")) {
        camera.status_info = CameraStatusInfo::FromJsonObject(obj.at("status_info").as_object());
    } else {
        camera.status_info.uuid = camera.base.uuid;
        if (obj.contains("status")) {
            std::string status_str = boost::json::value_to<std::string>(obj.at("status"));
            camera.status_info.status = StringToCameraStatus(status_str);
        }
        
        // 兼容时间格式
        if (obj.contains("last_online_time")) {
            if (obj.at("last_online_time").is_string()) {
                camera.status_info.last_online_time = StringToTimestamp(boost::json::value_to<std::string>(obj.at("last_online_time")));
            } else {
                camera.status_info.last_online_time = static_cast<int64_t>(obj.at("last_online_time").as_int64());
            }
        }
        if (obj.contains("offline_count")) camera.status_info.offline_count = static_cast<int>(obj.at("offline_count").as_int64());
        if (obj.contains("update_time")) {
            if (obj.at("update_time").is_string()) {
                camera.status_info.update_time = StringToTimestamp(boost::json::value_to<std::string>(obj.at("update_time")));
            } else {
                camera.status_info.update_time = static_cast<int64_t>(obj.at("update_time").as_int64());
            }
        }
    }
    
    return camera;
}

std::vector<CameraInfo> CameraInfo::FromJsonArray(const boost::json::array& arr) {
    std::vector<CameraInfo> cameras;
    for (const auto& item : arr) {
        if (item.is_object()) {
            cameras.push_back(FromJsonObject(item.as_object()));
        }
    }
    return cameras;
}
