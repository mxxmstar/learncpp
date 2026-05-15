#pragma once

#include "osd/osd_renderer.h"

/// @brief 形状渲染器基类
/// 
/// 定义通用形状绘制接口，各格式（YUV/RGB 等）派生子类实现。
/// plane0/plane1/plane2 为通用平面命名：
/// - YUV420: plane0=Y, plane1=U, plane2=V
/// - RGB:    plane0=RGB interleaved, plane1=nullptr, plane2=nullptr
class ShapeRenderer {
public:
    virtual ~ShapeRenderer() = default;
    
    /// @brief 绘制矩形
    virtual void DrawRect(
        uint8_t* plane0, uint8_t* plane1, uint8_t* plane2,
        int width, int height,
        int stride0, int stride1,
        const OsdRect& rect) = 0;
    
    /// @brief 绘制线段
    virtual void DrawLine(
        uint8_t* plane0, uint8_t* plane1, uint8_t* plane2,
        int width, int height,
        int stride0, int stride1,
        const OsdLine& line) = 0;
};