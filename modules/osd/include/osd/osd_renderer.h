#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <algorithm>

/// @brief OSD 颜色定义（YUV 格式）
struct OsdColorYuv {
    uint8_t y = 128;
    uint8_t u = 128;
    uint8_t v = 128;
    
    OsdColorYuv() = default;
    
    constexpr OsdColorYuv(uint8_t y_, uint8_t u_, uint8_t v_) noexcept
        : y(y_), u(u_), v(v_) {}
};

/// @brief OSD 边界框
struct OsdRect {
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;
    int thickness = 2;
    OsdColorYuv color;
};

/// @brief OSD 线段
struct OsdLine {
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;
    int thickness = 2;
    OsdColorYuv color;
};

/// @brief OSD 文本
struct OsdText {
    int x = 0;
    int y = 0;
    std::string text;
    OsdColorYuv color;
    int font_height = 16;
};

/// @brief OSD 配置
struct OsdConfig {
    bool enabled = false;
    int thickness = 2;
    int font_height = 16;
    int panel_margin = 10;
    bool show_channel_id = true;
    bool show_timestamp = true;
    bool show_fps = true;
    bool show_resolution = true;
};

/// @brief RGB 转 YUV (BT.601)
inline OsdColorYuv RgbToYuv(uint8_t r, uint8_t g, uint8_t b) {
    float y = 0.299f * r + 0.587f * g + 0.114f * b;
    float u = -0.147f * r - 0.289f * g + 0.436f * b + 128.0f;
    float v = 0.615f * r - 0.515f * g - 0.100f * b + 128.0f;
    return OsdColorYuv{
        static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, y))),
        static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, u))),
        static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, v)))
    };
}

/// @brief 预定义颜色
namespace OsdColors {
    static const OsdColorYuv Black{50, 128, 128};
    static const OsdColorYuv White{226, 128, 128};
    static const OsdColorYuv Green{149, 43, 21};
    static const OsdColorYuv Yellow{174, 0, 171};
    static const OsdColorYuv Red{65, 85, 255};
    static const OsdColorYuv Blue{41, 255, 113};
    static const OsdColorYuv Cyan{91, 0, 85};
    static const OsdColorYuv Magenta{65, 213, 255};
}

/// @brief OSD 渲染器基类
class OsdRenderer {
public:
    virtual ~OsdRenderer() = default;
    
    /// @brief 获取渲染器名称
    virtual const char* GetName() const = 0;
    
    /// @brief 获取配置
    virtual const OsdConfig& GetConfig() const = 0;
    
    /// @brief 重置状态
    virtual void Reset() {}
    
    /// @brief 设置通道 ID
    virtual void SetChannelId(int id) { channel_id_ = id; }
    
protected:
    int channel_id_ = 0;
    OsdConfig config_;
};

/// @brief YUV OSD 渲染器接口
class IYuvOsdRenderer : public OsdRenderer {
public:
    /// @brief 在 YUV420 帧上绘制检测框
    virtual void DrawRects(
        uint8_t* y_data, uint8_t* u_data, uint8_t* v_data,
        int width, int height,
        int y_stride, int uv_stride,
        const std::vector<OsdRect>& rects) = 0;
    
    /// @brief 在 YUV420 帧上绘制线条
    virtual void DrawLines(
        uint8_t* y_data, uint8_t* u_data, uint8_t* v_data,
        int width, int height,
        int y_stride, int uv_stride,
        const std::vector<OsdLine>& lines) = 0;
    
    /// @brief 在 YUV420 帧上绘制文本（简化版，仅绘制背景）
    virtual void DrawTexts(
        uint8_t* y_data, uint8_t* u_data, uint8_t* v_data,
        int width, int height,
        int y_stride, int uv_stride,
        const std::vector<OsdText>& texts) = 0;
    
    /// @brief 在 YUV420 帧上绘制半透明填充矩形
    virtual void DrawFilledRect(
        uint8_t* y_data, uint8_t* u_data, uint8_t* v_data,
        int width, int height,
        int y_stride, int uv_stride,
        int x, int y, int w, int h,
        float opacity) = 0;
};