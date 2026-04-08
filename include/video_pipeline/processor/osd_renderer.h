#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <chrono>

/// @brief OSD（On-Screen Display）屏幕显示模块
/// 用于在视频帧上绘制各种信息：检测框、FPS、时间戳等
class OsdRenderer {
public:
    OsdRenderer() = default;
    ~OsdRenderer() = default;
    
    /// @brief 在帧上绘制所有 OSD 元素
    /// @param frame 输入帧（会被修改）
    /// @param channel_id 通道 ID
    /// @param pts 时间戳（毫秒）
    /// @param fps 当前帧率
    /// @param detection_boxes 检测框列表 [x, y, width, height, label, confidence]
    void render(cv::Mat& frame, 
                int channel_id, 
                int64_t pts,
                float fps,
                const std::vector<std::tuple<int, int, int, int, std::string, float>>& detection_boxes = {});
    
private:
    /// @brief 绘制信息面板背景
    void drawInfoPanel(cv::Mat& frame, int channel_id, int64_t pts, float fps);
    
    /// @brief 绘制检测框
    void drawDetectionBoxes(cv::Mat& frame, 
                           const std::vector<std::tuple<int, int, int, int, std::string, float>>& boxes);
    
    /// @brief 格式化时间戳
    std::string formatTimestamp(int64_t pts_ms) const;
    
    /// @brief 根据置信度获取颜色
    cv::Scalar getColorByConfidence(float confidence) const;
    
    // FPS 计算相关
    std::chrono::steady_clock::time_point last_fps_update_;
    int frame_count_ = 0;
    float current_fps_ = 0.0f;
};
