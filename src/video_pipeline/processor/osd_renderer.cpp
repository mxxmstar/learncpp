#include "video_pipeline/processor/osd_renderer.h"
#include <sstream>
#include <iomanip>

void OsdRenderer::render(cv::Mat& frame, 
                         int channel_id, 
                         int64_t pts,
                         float fps,
                         const std::vector<std::tuple<int, int, int, int, std::string, float>>& detection_boxes) {
    if (frame.empty()) {
        return;
    }
    
    // 1. 绘制信息面板（左上角）
    drawInfoPanel(frame, channel_id, pts, fps);
    
    // 2. 绘制检测框
    if (!detection_boxes.empty()) {
        drawDetectionBoxes(frame, detection_boxes);
    }
}

void OsdRenderer::drawInfoPanel(cv::Mat& frame, int channel_id, int64_t pts, float fps) {
    // 计算面板尺寸
    int panel_width = 380;
    int panel_height = 90;
    int margin = 10;
    
    // 绘制半透明黑色背景
    cv::Rect panel_rect(margin, margin, panel_width, panel_height);
    cv::rectangle(frame, panel_rect, cv::Scalar(0, 0, 0), -1);
    cv::rectangle(frame, panel_rect, cv::Scalar(50, 50, 50), 1);
    
    // 添加透明度效果
    cv::Mat overlay;
    frame.copyTo(overlay);
    cv::rectangle(overlay, panel_rect, cv::Scalar(0, 0, 0), -1);
    cv::addWeighted(overlay, 0.5, frame, 0.5, 0, frame);
    
    // 绘制文本信息
    int font_face = cv::FONT_HERSHEY_SIMPLEX;
    double font_scale = 0.5;
    int thickness = 1;
    int line_height = 20;
    
    // 第 1 行：通道 ID
    std::string channel_text = "Channel: " + std::to_string(channel_id);
    cv::putText(frame, channel_text, 
               cv::Point(margin + 5, margin + line_height),
               font_face, font_scale, cv::Scalar(255, 255, 255), thickness);
    
    // 第 2 行：时间戳
    std::string time_text = "Time: " + formatTimestamp(pts);
    cv::putText(frame, time_text, 
               cv::Point(margin + 5, margin + line_height * 2),
               font_face, font_scale, cv::Scalar(255, 255, 255), thickness);
    
    // 第 3 行：FPS（带颜色）
    std::string fps_text = "FPS: " + std::to_string(static_cast<int>(fps));
    cv::Scalar fps_color = fps > 20 ? cv::Scalar(0, 255, 0) :   // 绿色：流畅
                          (fps > 10 ? cv::Scalar(0, 255, 255) :  // 黄色：一般
                                     cv::Scalar(0, 0, 255));      // 红色：卡顿
    cv::putText(frame, fps_text, 
               cv::Point(margin + 5, margin + line_height * 3),
               font_face, font_scale, fps_color, thickness);
    
    // 第 4 行：分辨率
    std::string res_text = "Resolution: " + std::to_string(frame.cols) + "x" + std::to_string(frame.rows);
    cv::putText(frame, res_text, 
               cv::Point(margin + 5, margin + line_height * 4),
               font_face, font_scale, cv::Scalar(200, 200, 200), thickness);
}

void OsdRenderer::drawDetectionBoxes(cv::Mat& frame, 
                                    const std::vector<std::tuple<int, int, int, int, std::string, float>>& boxes) {
    int thickness = 2;
    int font_face = cv::FONT_HERSHEY_SIMPLEX;
    double font_scale = 0.4;
    
    for (const auto& box : boxes) {
        int x = std::get<0>(box);
        int y = std::get<1>(box);
        int w = std::get<2>(box);
        int h = std::get<3>(box);
        std::string label = std::get<4>(box);
        float confidence = std::get<5>(box);
        
        // 根据置信度选择颜色
        cv::Scalar color = getColorByConfidence(confidence);
        
        // 绘制矩形框
        cv::rectangle(frame, cv::Rect(x, y, w, h), color, thickness);
        
        // 准备标签文本
        char label_buf[128];
        snprintf(label_buf, sizeof(label_buf), "%s: %.2f", label.c_str(), confidence);
        std::string label_text = label_buf;
        
        // 计算标签背景尺寸
        int baseline = 0;
        cv::Size text_size = cv::getTextSize(label_text, font_face, font_scale, 1, &baseline);
        
        // 绘制标签背景
        cv::rectangle(frame, 
                     cv::Rect(x, y - text_size.height - 5, text_size.width + 4, text_size.height + 5),
                     color, -1);
        
        // 绘制标签文本
        cv::putText(frame, label_text, 
                   cv::Point(x + 2, y - 2),
                   font_face, font_scale, cv::Scalar(255, 255, 255), 1);
    }
}

std::string OsdRenderer::formatTimestamp(int64_t pts_ms) const {
    // 将毫秒转换为 HH:MM:SS.mmm 格式
    int64_t total_seconds = pts_ms / 1000;
    int milliseconds = pts_ms % 1000;
    
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
    return std::string(buf);
}

cv::Scalar OsdRenderer::getColorByConfidence(float confidence) const {
    // 根据置信度返回颜色
    if (confidence >= 0.8f) {
        return cv::Scalar(0, 255, 0);      // 绿色：高置信度
    } else if (confidence >= 0.5f) {
        return cv::Scalar(0, 255, 255);    // 黄色：中等置信度
    } else {
        return cv::Scalar(0, 0, 255);      // 红色：低置信度
    }
}
