#pragma once

#include "osd/osd_renderer.h"
#include "osd/yuv/yuv_shape_renderer.h"
#include "osd/yuv/yuv_text_renderer.h"
#include "osd/yuv/yuv_blend.h"
#include <memory>
#include <chrono>

/// @brief YUV420 OSD 渲染器
class YuvOsdRenderer : public IYuvOsdRenderer {
public:
    explicit YuvOsdRenderer(const OsdConfig& config);
    ~YuvOsdRenderer() override = default;
    
    const char* GetName() const override { return "YuvOsdRenderer"; }
    const OsdConfig& GetConfig() const override { return config_; }
    void SetChannelId(int id) override { channel_id_ = id; }
    
    void DrawRects(
        uint8_t* y_data, uint8_t* u_data, uint8_t* v_data,
        int width, int height,
        int y_stride, int uv_stride,
        const std::vector<OsdRect>& rects) override;
    
    void DrawLines(
        uint8_t* y_data, uint8_t* u_data, uint8_t* v_data,
        int width, int height,
        int y_stride, int uv_stride,
        const std::vector<OsdLine>& lines) override;
    
    void DrawTexts(
        uint8_t* y_data, uint8_t* u_data, uint8_t* v_data,
        int width, int height,
        int y_stride, int uv_stride,
        const std::vector<OsdText>& texts) override;
    
    void DrawFilledRect(
        uint8_t* y_data, uint8_t* u_data, uint8_t* v_data,
        int width, int height,
        int y_stride, int uv_stride,
        int x, int y, int w, int h, float opacity) override;
    
    struct Detection {
        int class_id = -1;
        float confidence = 0.f;
        float x1 = 0.f, y1 = 0.f, x2 = 0.f, y2 = 0.f;
    };
    
    std::vector<OsdRect> BuildRects(const std::vector<Detection>& detections);
    
    struct InfoPanel {
        int x = 10, y = 10;
        int width = 300, height = 100;
        float opacity = 0.5f;
        std::vector<OsdText> lines;
    };
    
    InfoPanel BuildInfoPanel(int64_t pts, float fps, int channel_id, int width, int height);
    void SetShapeRenderer(std::shared_ptr<ShapeRenderer> r) { shape_renderer_ = r; }
    void SetTextRenderer(std::shared_ptr<TextRenderer> r) { text_renderer_ = r; }

private:
    static OsdColorYuv GetColorByConfidence(float confidence) {
        if (confidence >= 0.8f) return OsdColors::Green;
        if (confidence >= 0.5f) return OsdColors::Yellow;
        return OsdColors::Red;
    }
    static std::string FormatTimestamp(int64_t pts_ms);
    
    std::shared_ptr<ShapeRenderer> shape_renderer_;
    std::shared_ptr<TextRenderer> text_renderer_;
    
    std::chrono::steady_clock::time_point last_fps_update_;
    int frame_count_ = 0;
    float current_fps_ = 0.0f;
};