#pragma once
#include "camera/camera.h"
#include <memory>
#include <map>

class StreamSession;

class StreamManager {
public:
    static StreamManager& GetInstance();
    
    bool Init();
    void Shutdown();
    
    // 拉流控制
    bool StartStream(const CameraInfo& camera);
    bool StopStream(const std::string& camera_uuid);
    bool RestartStream(const std::string& camera_uuid);
    
    // 状态查询
    bool IsStreaming(const std::string& camera_uuid);
    StreamSession* GetSession(const std::string& camera_uuid);
    
    // 统计信息
    struct StreamStats {
        uint64_t total_bytes = 0;
        uint64_t total_frames = 0;
        double current_bitrate = 0.0;
        int64_t start_time = 0;
    };
    StreamStats GetStats(const std::string& camera_uuid);
    
private:
    StreamManager() = default;
    ~StreamManager() = default;
    
    std::map<std::string, std::unique_ptr<StreamSession>> sessions_;
    mutable std::mutex mutex_;
};

class StreamSession {
public:
    explicit StreamSession(const CameraInfo& camera);
    ~StreamSession();
    
    bool Start();
    bool Stop();
    
    const CameraInfo& GetCamera() const { return camera_; }
    bool IsRunning() const { return running_; }
    StreamManager::StreamStats GetStats() const;
    
private:
    void OnDataReceived(const uint8_t* data, size_t size);
    void OnError(const std::string& error);
    
    CameraInfo camera_;
    bool running_ = false;
    std::unique_ptr<class FFmpegDemuxer> demuxer_;
    
    // 统计
    uint64_t total_bytes_ = 0;
    uint64_t total_frames_ = 0;
    int64_t start_time_ = 0;
};