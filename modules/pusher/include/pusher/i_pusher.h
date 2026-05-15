#pragma once

#include "pusher/pusher_config.h"
#include <functional>
#include <memory>
#include <cstdint>

class IPusher {
public:
    using PushCallback = std::function<void(bool success, const std::string& msg, const PusherStats& stats)>;

    virtual ~IPusher() = default;

    virtual bool Start(const PusherConfig& config, PushCallback cb = nullptr) = 0;
    virtual void Stop() = 0;

    virtual bool PushYuvFrame(
        const uint8_t* y_data, const uint8_t* u_data, const uint8_t* v_data,
        int width, int height, int y_stride, int uv_stride,
        int64_t pts = 0) = 0;

    virtual bool IsRunning() const = 0;
    virtual const PusherStats& GetStats() const = 0;
    virtual void SetPushTimeout(int timeout_ms) = 0;

    static std::unique_ptr<IPusher> Create();
};