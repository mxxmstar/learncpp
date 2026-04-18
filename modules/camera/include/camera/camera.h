#pragma once
#include <boost/json.hpp>
#include <string>
#include <ctime>
#include <vector>
#include <cstdint>

// ==================== 枚举定义 ====================

enum class CameraStatus {
    Offline = 0,
    Online = 1,
    Streaming = 2
};

inline std::string CameraStatusToString(CameraStatus status) {
    switch (status) {
        case CameraStatus::Offline: return "offline";
        case CameraStatus::Online: return "online";
        case CameraStatus::Streaming: return "streaming";
        default: return "unknown";
    }
}

inline CameraStatus StringToCameraStatus(const std::string& str) {
    if (str == "online") return CameraStatus::Online;
    if (str == "streaming") return CameraStatus::Streaming;
    return CameraStatus::Offline;
}

// ==================== 数据结构拆分 ====================

/// @brief 摄像头基本信息（cameras_base 表）
struct CameraBaseInfo {
    std::string uuid;               // 摄像头唯一标识符（主键）
    std::string name;               // 摄像头名称
    std::string vendor;             // 厂商（hikvision/dahua/uniview等）
    std::string hardware;           // 硬件型号
    std::string software;           // 软件版本
    std::string serialnumber;       // 序列号
    std::string customer;           // 客户名称
    std::string metadata;           // JSON 格式的扩展元数据
    
    int64_t create_time = 0;        // 创建时间（Unix 时间戳，秒）
    int64_t update_time = 0;        // 更新时间（Unix 时间戳，秒）
    
    static boost::json::object ToJsonObject(const CameraBaseInfo& info);
    static CameraBaseInfo FromJsonObject(const boost::json::object& obj);
};

/// @brief 摄像头rtsp连接信息（cameras_connection 表）
struct CameraConnectionInfo {
    std::string uuid;               // 外键，关联 cameras_base.uuid
    std::string rtsp_url;           // RTSP 地址（主要）
    std::string username;           // 用户名
    std::string password;           // 密码（前期明文存储）
    
    static boost::json::object ToJsonObject(const CameraConnectionInfo& info);
    static CameraConnectionInfo FromJsonObject(const boost::json::object& obj);
};

/// @brief 摄像头协议配置（cameras_protocol 表）
struct CameraProtocolInfo {
    std::string uuid;               // 外键，关联 cameras_base.uuid
    std::string protocol_type;      // 协议类型: "onvif", "gb28181", "http_api", "manual"
    std::string http_base_url;      // HTTP API 基础 URL（可选）
    std::string onvif_device_url;   // ONVIF 设备 URL（可选）
    std::string gb28181_id;         // GB/T 28181 设备 ID（可选）
    
    static boost::json::object ToJsonObject(const CameraProtocolInfo& info);
    static CameraProtocolInfo FromJsonObject(const boost::json::object& obj);
};

/// @brief 摄像头视频参数（cameras_video_params 表）
struct CameraVideoParams {
    std::string uuid;               // 外键，关联 cameras_base.uuid
    int width = 1920;               // 分辨率宽度
    int height = 1080;              // 分辨率高度
    int fps = 25;                   // 帧率
    int bitrate = 4096;             // 码率（kbps）
    
    static boost::json::object ToJsonObject(const CameraVideoParams& params);
    static CameraVideoParams FromJsonObject(const boost::json::object& obj);
};

/// @brief 摄像头状态信息（cameras_status 表）
struct CameraStatusInfo {
    std::string uuid;               // 外键，关联 cameras_base.uuid
    CameraStatus status = CameraStatus::Offline;  // 状态: Offline/Online/Streaming
    int64_t last_online_time = 0;   // 最后在线时间（Unix 时间戳，秒）
    int offline_count = 0;          // 离线次数统计
    int64_t update_time = 0;        // 状态更新时间（Unix 时间戳，秒）
    
    static boost::json::object ToJsonObject(const CameraStatusInfo& info);
    static CameraStatusInfo FromJsonObject(const boost::json::object& obj);
};

/// @brief 完整摄像头信息（组合所有子结构）
struct CameraInfo {
    CameraBaseInfo base;            // 基本信息
    CameraConnectionInfo connection; // 连接信息
    CameraProtocolInfo protocol;    // 协议配置
    CameraVideoParams video_params; // 视频参数
    CameraStatusInfo status_info;   // 状态信息
    
    // 便捷访问方法
    const std::string& GetUuid() const { return base.uuid; }
    const std::string& GetName() const { return base.name; }
    const std::string& GetRtspUrl() const { return connection.rtsp_url; }
    CameraStatus GetStatus() const { return status_info.status; }
    
    // JSON 序列化
    static boost::json::object ToJsonObject(const CameraInfo& camera);
    static boost::json::array ToJsonArray(const std::vector<CameraInfo>& cameras);
    static CameraInfo FromJsonObject(const boost::json::object& obj);
    static std::vector<CameraInfo> FromJsonArray(const boost::json::array& arr);
};

std::string CameraListToJsonObject(const std::vector<CameraInfo>& cameras);
