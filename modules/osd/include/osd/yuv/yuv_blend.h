#pragma once

#include "osd/blend_engine.h"
#include <algorithm>
#include <cstring>

/// @brief Alpha 混合引擎（YUV420 实现）
class AlphaBlendEngine : public BlendEngine {
public:
    void SetOpacity(float opacity) override { opacity_ = opacity; }
    float GetOpacity() const override { return opacity_; }
    
    static uint8_t BlendPixel(uint8_t src, uint8_t dst, float alpha) {
        return static_cast<uint8_t>(src * alpha + dst * (1.0f - alpha));
    }
    
    static void FillPlane(
        uint8_t* data, int stride,
        int x, int y, int w, int h,
        uint8_t value) {
        
        x = std::max(0, x);
        y = std::max(0, y);
        for (int j = y; j < y + h && j * stride < stride * h; ++j) {
            std::fill_n(data + j * stride + x, w, value);
        }
    }
    
    static void BlendRectY(
        uint8_t* y_data, int width, int height, int y_stride,
        int x, int y, int w, int h,
        uint8_t src_y, float alpha) {
        
        x = std::max(0, x);
        y = std::max(0, y);
        int x2 = std::min(x + w, width);
        int y2 = std::min(y + h, height);
        float inv_a = 1.0f - alpha;
        
        for (int j = y; j < y2; ++j) {
            for (int i = x; i < x2; ++i) {
                y_data[j * y_stride + i] = 
                    static_cast<uint8_t>(src_y * alpha + y_data[j * y_stride + i] * inv_a);
            }
        }
    }
    
    static void BlendRectUV(
        uint8_t* uv_data, int width, int height, int uv_stride,
        int x, int y, int w, int h,
        uint8_t src_uv, float alpha) {
        
        int x1 = x / 2;
        int y1 = y / 2;
        int x2 = (x + w + 1) / 2;
        int y2 = (y + h + 1) / 2;
        
        x1 = std::max(0, x1);
        y1 = std::max(0, y1);
        x2 = std::min(x2, (width + 1) / 2);
        y2 = std::min(y2, (height + 1) / 2);
        float inv_a = 1.0f - alpha;
        
        for (int j = y1; j < y2; ++j) {
            for (int i = x1; i < x2; ++i) {
                uv_data[j * uv_stride + i] = 
                    static_cast<uint8_t>(src_uv * alpha + uv_data[j * uv_stride + i] * inv_a);
            }
        }
    }
    
    static void SetPixelY(uint8_t* y_data, int y_stride, int x, int y, uint8_t value) {
        y_data[y * y_stride + x] = value;
    }
    
    static void SetPixelUV(uint8_t* u_data, uint8_t* v_data, int uv_stride,
                           int x, int y, uint8_t u_val, uint8_t v_val) {
        if (x % 2 == 0 && y % 2 == 0) {
            int uv_x = x / 2;
            int uv_y = y / 2;
            u_data[uv_y * uv_stride + uv_x] = u_val;
            v_data[uv_y * uv_stride + uv_x] = v_val;
        }
    }

private:
    float opacity_ = 1.0f;
};