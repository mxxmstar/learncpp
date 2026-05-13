#pragma once

#include "pusher/pusher_config.h"
#include <opencv2/opencv.hpp>
#include <functional>
#include <memory>

class IPusher {
public:
    using PushCallback = std::function<void(bool success, const std::string& msg, const PusherStats& stats)>;
    
    virtual ~IPusher() = default;
    
    virtual bool Start(const PusherConfig& config, PushCallback cb = nullptr) = 0;
    virtual void Stop() = 0;
    virtual bool PushFrame(const cv::Mat& bgr_frame, int64_t pts = 0) = 0;
    virtual bool IsRunning() const = 0;
    virtual const PusherStats& GetStats() const = 0;
    
    static std::unique_ptr<IPusher> Create();
};