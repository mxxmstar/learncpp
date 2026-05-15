#pragma once

#include "osd/osd_renderer.h"

/// @brief 文本渲染器基类
/// 
/// 定义通用文本绘制接口，各格式（YUV/RGB 等）派生子类实现。
class TextRenderer {
public:
    virtual ~TextRenderer() = default;
    
    /// @brief 绘制文本
    /// @param plane0 平面 0 数据指针
    /// @param plane1 平面 1 数据指针（YUV 的 U / 无）
    /// @param plane2 平面 2 数据指针（YUV 的 V / 无）
    /// @param width 帧宽度
    /// @param height 帧高度
    /// @param stride0 平面 0 行步长
    /// @param stride1 平面 1 行步长
    /// @param text 文本参数
    virtual void DrawText(
        uint8_t* plane0, uint8_t* plane1, uint8_t* plane2,
        int width, int height, int stride0, int stride1,
        const OsdText& text) = 0;
};