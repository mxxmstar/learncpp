#pragma once
#include <boost/json.hpp>
#include <string>
#include <ctime>
#include <vector>

enum class CameraStatus {
    Offline = 0,
    Online = 1,
    Streaming = 2
};

std::string CameraStatusToString(CameraStatus status) {
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

struct CameraInfo {
    std::string uuid;               // 摄像头唯一标识符
    std::string name;               // 摄像头名称
    std::string vendor;             // 厂商
    std::string hardware;           // 硬件型号
    std::string software;           // 软件版本
    std::string serialnumber;       // 序列号
    std::string customer;           // 客户名称
    std::string metadata;           // 其他元数据

    std::string rtsp_url;           // RTSP 地址
    std::string username;           // 用户名
    std::string password;           // 密码

    int width;                      // 分辨率宽度
    int height;                   // 分辨率高度
    int fps;                      // 帧率
    CameraStatus status;          // 状态（在线/离线/推流中）
    std::string create_time;      // 创建时间
    std::string update_time;      // 更新时间

    boost::json::object ToJsonObject() const;
    
};

std::string CameraListToJsonObject(const std::vector<CameraInfo>& cameras);
