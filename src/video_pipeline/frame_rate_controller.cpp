#include "video_pipeline/frame_rate_controller.h"
#include <sstream>
#include <iomanip>

namespace video_pipeline {

FrameRateController::FrameRateController(int target_fps)
    : target_fps_(target_fps)
    , last_frame_time_(std::chrono::steady_clock::now()) {
}

bool FrameRateController::shouldSendFrame() {
    int fps = target_fps_.load();
    
    // 如果目标 FPS 为 0，不限制帧率
    if (fps <= 0) {
        return true;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_frame_time_).count();
    
    // 计算目标帧间隔（毫秒）
    int target_interval_ms = 1000 / fps;
    
    // 如果时间间隔不够，跳过这一帧
    if (elapsed_ms < target_interval_ms) {
        recordFrameSkipped();
        return false;
    }
    
    // 更新最后发送时间
    last_frame_time_ = now;
    return true;
}

void FrameRateController::recordFrameSent() {
    total_sent_++;
    
    // 记录到最近帧窗口
    {
        std::lock_guard<std::mutex> lock(recent_mutex_);
        recent_frames_.timestamps[recent_frames_.index] = 
            std::chrono::steady_clock::now();
        recent_frames_.index = (recent_frames_.index + 1) % RECENT_FRAMES_WINDOW;
        if (recent_frames_.count < RECENT_FRAMES_WINDOW) {
            recent_frames_.count++;
        }
    }
}

void FrameRateController::recordFrameSkipped() {
    total_skipped_++;
}

void FrameRateController::setTargetFps(int fps) {
    if (fps < 0) {
        fps = 0;
    }
    target_fps_ = fps;
}

double FrameRateController::getActualFps() const {
    std::lock_guard<std::mutex> lock(recent_mutex_);
    
    if (recent_frames_.count < 2) {
        return 0.0;
    }
    
    // 找到最早和最晚的时间戳
    auto now = std::chrono::steady_clock::now();
    int start_idx = (recent_frames_.index - recent_frames_.count + RECENT_FRAMES_WINDOW) % RECENT_FRAMES_WINDOW;
    
    auto earliest = recent_frames_.timestamps[start_idx];
    auto latest = now;
    
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        latest - earliest).count();
    
    if (duration_ms <= 0) {
        return 0.0;
    }
    
    // 计算 FPS
    double duration_sec = duration_ms / 1000.0;
    return recent_frames_.count / duration_sec;
}

double FrameRateController::getSkipRate() const {
    uint64_t sent = total_sent_.load();
    uint64_t skipped = total_skipped_.load();
    uint64_t total = sent + skipped;
    
    if (total == 0) {
        return 0.0;
    }
    
    return (static_cast<double>(skipped) / total) * 100.0;
}

void FrameRateController::resetStats() {
    total_sent_ = 0;
    total_skipped_ = 0;
    
    std::lock_guard<std::mutex> lock(recent_mutex_);
    recent_frames_.count = 0;
    recent_frames_.index = 0;
}

std::string FrameRateController::getStatsString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "FPS Controller: target=" << target_fps_.load()
        << ", actual=" << getActualFps()
        << ", sent=" << total_sent_.load()
        << ", skipped=" << total_skipped_.load()
        << ", skip_rate=" << getSkipRate() << "%";
    return oss.str();
}

} // namespace video_pipeline
