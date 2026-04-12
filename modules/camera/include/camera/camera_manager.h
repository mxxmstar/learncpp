#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>
#include "camera/camera_storage.h"
#include "camera/camera.h"

class CameraDevice;
class CameraManager {
public:
    using StatusCallback = std::function<void(const std::string& uuid, CameraStatus status)>;
    
    static CameraManager& GetInstance();
    
    bool Init();
    void Shutdown();
    
    // 注册/注销
    bool Register(const CameraInfo& camera);
    bool Unregister(const std::string& uuid);
    
    // 获取摄像头
    std::shared_ptr<CameraDevice> GetCamera(const std::string& uuid);
    std::vector<std::shared_ptr<CameraDevice>> GetAllCameras();
    
    // 批量操作
    bool StartAllCameras();
    bool StopAllCameras();
    
    // 状态监控
    void SetStatusCallback(StatusCallback callback);
    
private:
    CameraManager() = default;
    ~CameraManager() = default;
    
    std::map<std::string, std::shared_ptr<CameraDevice>> cameras_;
    StatusCallback status_callback_;
    mutable std::mutex mutex_;
};


class CameraDevice {
public:
    explicit CameraDevice(const CameraInfo& camera);
    ~CameraDevice();
    
    // 基本信息
    const CameraInfo& GetInfo() const { return camera_; }
    CameraStatus GetStatus() const { return status_; }
    
    // 生命周期管理
    bool Start();
    bool Stop();
    
    // 参数调整（通过 HTTP API）
    bool SetResolution(int width, int height);
    bool SetFPS(int fps);
    bool SetBitrate(int bitrate);
    
    // 云台控制
    bool PTZControl(int direction, int speed);
    bool ZoomIn();
    bool ZoomOut();
    
    // 截图
    bool CaptureSnapshot(const std::string& output_path);
    
    // 重启/重置
    bool Reboot();
    bool ResetFactory();
    
private:
    bool SendCommand(const std::string& api, const boost::json::object& params);
    void UpdateStatus(CameraStatus status);
    
    CameraInfo camera_;
    CameraStatus status_ = CameraStatus::Offline;
    std::unique_ptr<class CameraHttpClient> http_client_;
    std::unique_ptr<class StreamSession> stream_session_;
};