#pragma once

#include <string>
#include <cstdint>

struct PusherConfig {
    std::string url;
    int width = 1920;
    int height = 1080;
    int fps = 25;
    int bitrate = 2000;
    int gop_size = 50;
    std::string codec = "h264";
    
    bool isValid() const {
        return !url.empty() && width > 0 && height > 0 && fps > 0;
    }
};

struct PusherStats {
    uint64_t frames_sent = 0;
    uint64_t frames_failed = 0;
    uint64_t bytes_sent = 0;
    int64_t last_pts = 0;
    
    float getSuccessRate() const {
        uint64_t total = frames_sent + frames_failed;
        return total > 0 ? static_cast<float>(frames_sent) / total * 100.0f : 0.0f;
    }
};