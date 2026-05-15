#pragma once

#include "pusher/i_pusher.h"
#include <atomic>
#include <thread>
#include <vector>

class YuvPusher : public IPusher {
public:
    YuvPusher();
    ~YuvPusher() override;

    bool Start(const PusherConfig& config, PushCallback cb = nullptr) override;
    void Stop() override;

    bool PushYuvFrame(
        const uint8_t* y_plane, const uint8_t* u_plane, const uint8_t* v_plane,
        int width, int height, int y_stride, int uv_stride,
        int64_t pts = 0) override;

    bool IsRunning() const override { return running_; }
    const PusherStats& GetStats() const override { return stats_; }
    void SetPushTimeout(int timeout_ms) { push_timeout_ms_ = timeout_ms; }

private:
    void runLoop();
    bool writeToStdin(const uint8_t* data, size_t size);

    PusherConfig config_;
    PusherStats stats_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
    int push_timeout_ms_ = 30;

#ifdef _WIN32
    void* stdin_pipe_ = nullptr;
    void* ffmpeg_process_ = nullptr;
#else
    int stdin_pipe_ = -1;
    pid_t ffmpeg_pid_ = -1;
#endif

    std::thread io_thread_;
    PushCallback callback_;
};