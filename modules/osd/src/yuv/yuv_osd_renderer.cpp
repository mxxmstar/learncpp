#include "osd/yuv/yuv_osd_renderer.h"
#include "common/log/logmanager.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>

YuvOsdRenderer::YuvOsdRenderer(const OsdConfig& config)
    : last_fps_update_(std::chrono::steady_clock::now()) {
    
    config_ = config;
    
    shape_renderer_ = std::make_shared<YuvShapeRenderer>();
    text_renderer_ = std::make_shared<SimpleTextRenderer>();
    
    LOG_MAIN_INFO_AT("Creating YuvOsdRenderer:");
    LOG_MAIN_INFO_AT("  enabled: {}", config_.enabled ? "yes" : "no");
    LOG_MAIN_INFO_AT("  thickness: {}", config_.thickness);
}

void YuvOsdRenderer::DrawRects(
    uint8_t* y_data, uint8_t* u_data, uint8_t* v_data,
    int width, int height,
    int y_stride, int uv_stride,
    const std::vector<OsdRect>& rects) {
    
    if (!config_.enabled || !shape_renderer_ || !y_data) {
        return;
    }
    
    for (const auto& rect : rects) {
        shape_renderer_->DrawRect(y_data, u_data, v_data, 
            width, height, y_stride, uv_stride, rect);
    }
}

void YuvOsdRenderer::DrawLines(
    uint8_t* y_data, uint8_t* u_data, uint8_t* v_data,
    int width, int height,
    int y_stride, int uv_stride,
    const std::vector<OsdLine>& lines) {
    
    if (!config_.enabled || !shape_renderer_ || !y_data) {
        return;
    }
    
    for (const auto& line : lines) {
        shape_renderer_->DrawLine(y_data, u_data, v_data,
            width, height, y_stride, uv_stride, line);
    }
}

void YuvOsdRenderer::DrawTexts(
    uint8_t* y_data, uint8_t* u_data, uint8_t* v_data,
    int width, int height,
    int y_stride, int uv_stride,
    const std::vector<OsdText>& texts) {
    
    if (!config_.enabled || !text_renderer_ || !y_data) {
        return;
    }
    
    for (const auto& text : texts) {
        text_renderer_->DrawText(y_data, u_data, v_data,
            width, height, y_stride, uv_stride, text);
    }
}

void YuvOsdRenderer::DrawFilledRect(
    uint8_t* y_data, uint8_t* u_data, uint8_t* v_data,
    int width, int height,
    int y_stride, int uv_stride,
    int x, int y, int w, int h,
    float opacity) {
    
    if (!y_data) return;
    
    // 裁剪到有效范围
    x = std::max(0, x);
    y = std::max(0, y);
    int x2 = std::min(x + w, width);
    int y2 = std::min(y + h, height);
    
    if (x2 <= x || y2 <= y) return;
    
    // 绘制半透明黑色背景
    AlphaBlendEngine::BlendRectY(
        y_data, width, height, y_stride,
        x, y, x2 - x, y2 - y, 50, opacity);
    
    AlphaBlendEngine::BlendRectUV(
        u_data, width, height, uv_stride,
        x, y, x2 - x, y2 - y, 128, opacity);
    
    AlphaBlendEngine::BlendRectUV(
        v_data, width, height, uv_stride,
        x, y, x2 - x, y2 - y, 128, opacity);
}

std::vector<OsdRect> YuvOsdRenderer::BuildRects(
    const std::vector<Detection>& detections) {
    
    std::vector<OsdRect> rects;
    rects.reserve(detections.size());
    
    for (const auto& det : detections) {
        OsdRect rect;
        rect.x1 = static_cast<int>(det.x1);
        rect.y1 = static_cast<int>(det.y1);
        rect.x2 = static_cast<int>(det.x2);
        rect.y2 = static_cast<int>(det.y2);
        rect.thickness = config_.thickness;
        rect.color = GetColorByConfidence(det.confidence);
        rects.push_back(rect);
    }
    
    return rects;
}

YuvOsdRenderer::InfoPanel YuvOsdRenderer::BuildInfoPanel(
    int64_t pts, float fps, 
    int channel_id, int width, int height) {
    
    InfoPanel panel;
    panel.x = config_.panel_margin;
    panel.y = config_.panel_margin;
    panel.width = 320;
    panel.height = config_.show_resolution ? 100 : 80;
    
    int line_idx = 0;
    int line_height = 18;
    
    if (config_.show_channel_id) {
        OsdText line;
        line.x = panel.x + 5;
        line.y = panel.y + 15 + line_idx * line_height;
        line.text = "Channel: " + std::to_string(channel_id);
        line.color = OsdColors::White;
        panel.lines.push_back(line);
        line_idx++;
    }
    
    if (config_.show_timestamp) {
        OsdText line;
        line.x = panel.x + 5;
        line.y = panel.y + 15 + line_idx * line_height;
        line.text = "Time: " + FormatTimestamp(pts);
        line.color = OsdColors::White;
        panel.lines.push_back(line);
        line_idx++;
    }
    
    if (config_.show_fps) {
        OsdText line;
        line.x = panel.x + 5;
        line.y = panel.y + 15 + line_idx * line_height;
        line.text = "FPS: " + std::to_string(static_cast<int>(fps));
        line.color = (fps > 20) ? OsdColors::Green : 
                    (fps > 10) ? OsdColors::Yellow : OsdColors::Red;
        panel.lines.push_back(line);
        line_idx++;
    }
    
    if (config_.show_resolution) {
        OsdText line;
        line.x = panel.x + 5;
        line.y = panel.y + 15 + line_idx * line_height;
        line.text = "Resolution: " + std::to_string(width) + "x" + std::to_string(height);
        line.color = OsdColors::White;
        panel.lines.push_back(line);
    }
    
    return panel;
}

std::string YuvOsdRenderer::FormatTimestamp(int64_t pts_ms) {
    int64_t total_seconds = pts_ms / 1000;
    int milliseconds = pts_ms % 1000;
    
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", 
        hours, minutes, seconds, milliseconds);
    return std::string(buf);
}