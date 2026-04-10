#pragma once

#include <chrono>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace video_pipeline {

/**
 * @brief 帧率控制器
 * 
 * 功能：
 * - 控制帧发送频率
 * - 自动跳帧以达到目标 FPS
 * - 提供统计信息（实际 FPS、跳帧率等）
 * - 线程安全
 */
class FrameRateController {
public:
    /**
     * @brief 构造函数
     * @param target_fps 目标帧率（0 表示不限制）
     */
    explicit FrameRateController(int target_fps = 10);
    
    /// @brief 析构函数
    ~FrameRateController() = default;
    
    // 禁止拷贝
    FrameRateController(const FrameRateController&) = delete;
    FrameRateController& operator=(const FrameRateController&) = delete;
    
    /**
     * @brief 检查是否应该发送当前帧
     * @return true 应该发送，false 应该跳过
     */
    bool shouldSendFrame();
    
    /**
     * @brief 记录一帧已发送
     */
    void recordFrameSent();
    
    /**
     * @brief 记录一帧被跳过
     */
    void recordFrameSkipped();
    
    /**
     * @brief 设置目标帧率
     * @param fps 目标帧率（0 表示不限制）
     */
    void setTargetFps(int fps);
    
    /**
     * @brief 获取目标帧率
     */
    int getTargetFps() const { return target_fps_.load(); }
    
    /**
     * @brief 获取实际帧率（最近 5 秒的平均值）
     */
    double getActualFps() const;
    
    /**
     * @brief 获取总发送帧数
     */
    uint64_t getTotalSent() const { return total_sent_.load(); }
    
    /**
     * @brief 获取总跳过帧数
     */
    uint64_t getTotalSkipped() const { return total_skipped_.load(); }
    
    /**
     * @brief 获取跳帧率（百分比）
     */
    double getSkipRate() const;
    
    /**
     * @brief 重置统计信息
     */
    void resetStats();
    
    /**
     * @brief 获取统计信息字符串
     */
    std::string getStatsString() const;

private:
    /// @brief 目标帧率
    std::atomic<int> target_fps_;
    
    /// @brief 上一帧发送时间
    std::chrono::steady_clock::time_point last_frame_time_;
    
    /// @brief 统计信息互斥锁
    mutable std::mutex stats_mutex_;
    
    /// @brief 总发送帧数
    std::atomic<uint64_t> total_sent_{0};
    
    /// @brief 总跳过帧数
    std::atomic<uint64_t> total_skipped_{0};
    
    /// @brief 最近发送的时间戳（用于计算实际 FPS）
    static constexpr int RECENT_FRAMES_WINDOW = 5; // 5 秒窗口
    struct RecentFrames {
        std::chrono::steady_clock::time_point timestamps[RECENT_FRAMES_WINDOW];
        int count = 0;
        int index = 0;
    };
    mutable std::mutex recent_mutex_;
    RecentFrames recent_frames_;
};

} // namespace video_pipeline
