#pragma once

#include "pusher/i_pusher.h"
#include "pusher/bgr_to_yuv_converter.h"
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>

class YuvPusher : public IPusher {
public:
    YuvPusher();
    ~YuvPusher() override;
    
    bool Start(const PusherConfig& config, PushCallback cb = nullptr) override;
    void Stop() override;
    bool PushFrame(const cv::Mat& bgr_frame, int64_t pts = 0) override;
    bool IsRunning() const override { return running_; }
    const PusherStats& GetStats() const override { return stats_; }
    
private:
    bool initFFmpeg();
    void runLoop();
    bool writeToStdin(const uint8_t* data, size_t size);
    
    PusherConfig config_;
    PusherStats stats_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
    
    BgrToYuvConverter converter_;
    std::vector<uint8_t> yuv_buffer_;
    
#ifdef _WIN32
    void* stdin_pipe_ = nullptr;
    void* ffmpeg_process_ = nullptr;
#else
    int stdin_pipe_ = -1;
    pid_t ffmpeg_pid_ = -1;
#endif
    
    std::thread io_thread_;
    std::mutex queue_mutex_;
    std::queue<std::pair<cv::Mat, int64_t>> frame_queue_;
    PushCallback callback_;
};