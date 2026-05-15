#pragma once

#include "osd/shape_renderer.h"
#include "osd/yuv/yuv_blend.h"

/// @brief YUV 形状渲染器（YUV420 格式）
class YuvShapeRenderer : public ShapeRenderer {
public:
    void DrawRect(
        uint8_t* plane0, uint8_t* plane1, uint8_t* plane2,
        int width, int height,
        int stride0, int stride1,
        const OsdRect& rect) override {
        
        uint8_t* y = plane0;
        uint8_t* u = plane1;
        uint8_t* v = plane2;
        int y_stride = stride0;
        int uv_stride = stride1;
        
        int x1 = std::max(0, rect.x1);
        int y1 = std::max(0, rect.y1);
        int x2 = std::min(rect.x2, width);
        int y2 = std::min(rect.y2, height);
        int t = rect.thickness;
        
        for (int i = x1; i < x2; ++i) {
            for (int th = 0; th < t && (y1 + th) < y2; ++th) {
                AlphaBlendEngine::SetPixelY(y, y_stride, i, y1 + th, rect.color.y);
                AlphaBlendEngine::SetPixelUV(u, v, uv_stride, i, y1 + th, rect.color.u, rect.color.v);
            }
        }
        for (int i = x1; i < x2; ++i) {
            for (int th = 0; th < t && (y2 - 1 - th) >= y1; ++th) {
                AlphaBlendEngine::SetPixelY(y, y_stride, i, y2 - 1 - th, rect.color.y);
                AlphaBlendEngine::SetPixelUV(u, v, uv_stride, i, y2 - 1 - th, rect.color.u, rect.color.v);
            }
        }
        for (int j = y1; j < y2; ++j) {
            for (int th = 0; th < t && (x1 + th) < x2; ++th) {
                AlphaBlendEngine::SetPixelY(y, y_stride, x1 + th, j, rect.color.y);
                AlphaBlendEngine::SetPixelUV(u, v, uv_stride, x1 + th, j, rect.color.u, rect.color.v);
            }
        }
        for (int j = y1; j < y2; ++j) {
            for (int th = 0; th < t && (x2 - 1 - th) >= x1; ++th) {
                AlphaBlendEngine::SetPixelY(y, y_stride, x2 - 1 - th, j, rect.color.y);
                AlphaBlendEngine::SetPixelUV(u, v, uv_stride, x2 - 1 - th, j, rect.color.u, rect.color.v);
            }
        }
    }
    
    void DrawLine(
        uint8_t* plane0, uint8_t* plane1, uint8_t* plane2,
        int width, int height,
        int stride0, int stride1,
        const OsdLine& line) override {
        
        uint8_t* y = plane0;
        uint8_t* u = plane1;
        uint8_t* v = plane2;
        int y_stride = stride0;
        int uv_stride = stride1;
        
        int x0 = line.x1, y0 = line.y1;
        int x1 = line.x2, y1 = line.y2;
        int dx = abs(x1 - x0), dy = abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;
        
        while (true) {
            if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height) {
                AlphaBlendEngine::SetPixelY(y, y_stride, x0, y0, line.color.y);
                AlphaBlendEngine::SetPixelUV(u, v, uv_stride, x0, y0, line.color.u, line.color.v);
            }
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx) { err += dx; y0 += sy; }
        }
    }
};