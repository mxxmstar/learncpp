#pragma once

#include "videopipeline/i_algorithm_backend.h"
#include "common/log/logmanager.h"

/// @brief 空实现后端（用于测试和占位）
/// 
/// 不做任何处理，仅用于：
/// 1. 测试 VideoPipeline 框架
/// 2. 作为默认后端
/// 3. 开发时的占位符
class NullBackend : public IAlgorithmBackend {
public:
    NullBackend() = default;
    ~NullBackend() override = default;
    
    bool initialize(const AlgorithmConfig& config) override {
        LOG_MAIN_INFO_AT("[NullBackend] Initialized (no-op backend)");
        initialized_ = true;
        return true;
    }
    
    void processFrame(const VideoFrame& frame) override {
        // 不做任何处理
        if (result_callback_) {
            DetectionResult result;
            result.channel_id = 0;
            result.timestamp = frame.pts;
            result_callback_(0, result);
        }
    }
    
    void processFrame(cv::Mat&& frame, int64_t pts) override {
        // 不做任何处理
        if (result_callback_) {
            DetectionResult result;
            result.channel_id = 0;
            result.timestamp = pts;
            result_callback_(0, result);
        }
    }
    
    std::string getBackendType() const override {
        return "none";
    }
    
    bool isInitialized() const override {
        return initialized_;
    }
    
    void stop() override {
        initialized_ = false;
        LOG_MAIN_INFO_AT("[NullBackend] Stopped");
    }
    
private:
    bool initialized_ = false;
};
