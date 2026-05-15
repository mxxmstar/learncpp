#pragma once

#include "osd/text_renderer.h"
#include "osd/yuv/yuv_blend.h"
#include <unordered_map>
#include <array>

using FontBitmap = std::array<uint8_t, 5>;

/// @brief 简单文本渲染器（仅绘制半透明背景块）
class SimpleTextRenderer : public TextRenderer {
public:
    void DrawText(
        uint8_t* plane0, uint8_t* plane1, uint8_t* plane2,
        int width, int height, int stride0, int stride1,
        const OsdText& text) override {
        
        int text_width = static_cast<int>(text.text.length()) * text.font_height / 2;
        int text_height = text.font_height;
        
        AlphaBlendEngine::BlendRectY(plane0, width, height, stride0,
            text.x, text.y, text_width, text_height, text.color.y, 0.7f);
        AlphaBlendEngine::BlendRectUV(plane1, width, height, stride1,
            text.x, text.y, text_width, text_height, text.color.u, 0.7f);
        AlphaBlendEngine::BlendRectUV(plane2, width, height, stride1,
            text.x, text.y, text_width, text_height, text.color.v, 0.7f);
    }
};

/// @brief 像素字体渲染器
class PixelFontRenderer : public TextRenderer {
public:
    static const std::unordered_map<char, FontBitmap> kPixelFonts;
    
    void DrawText(
        uint8_t* plane0, uint8_t* plane1, uint8_t* plane2,
        int width, int height, int stride0, int stride1,
        const OsdText& text) override {
        
        int char_width = 6, char_height = 8;
        
        for (size_t i = 0; i < text.text.length(); ++i) {
            char c = text.text[i];
            if (c < 32 || c > 126) continue;
            auto it = kPixelFonts.find(c);
            if (it == kPixelFonts.end()) continue;
            
            const FontBitmap& bm = it->second;
            for (int row = 0; row < char_height && (text.y + row) < height; ++row) {
                for (int col = 0; col < char_width && (text.x + (int)i * char_width + col) < width; ++col) {
                    if (bm[row] & (1 << (char_width - 1 - col))) {
                        AlphaBlendEngine::SetPixelY(plane0, stride0,
                            text.x + (int)i * char_width + col, text.y + row, text.color.y);
                    }
                }
            }
        }
    }
};

inline const std::unordered_map<char, FontBitmap> PixelFontRenderer::kPixelFonts = {
    {'0', FontBitmap{0x7C, 0x82, 0x82, 0x82, 0x7C}},
    {'1', FontBitmap{0x00, 0x84, 0xFE, 0x80, 0x00}},
    {'2', FontBitmap{0xC4, 0xA2, 0x92, 0x92, 0x8C}},
    {'3', FontBitmap{0x44, 0x82, 0x92, 0x92, 0x6C}},
    {'4', FontBitmap{0x30, 0x28, 0x24, 0xFE, 0x20}},
    {'5', FontBitmap{0x4E, 0x8A, 0x8A, 0x8A, 0x72}},
    {'6', FontBitmap{0x78, 0x94, 0x92, 0x92, 0x60}},
    {'7', FontBitmap{0x02, 0xE2, 0x12, 0x0A, 0x06}},
    {'8', FontBitmap{0x6C, 0x92, 0x92, 0x92, 0x6C}},
    {'9', FontBitmap{0x0C, 0x92, 0x92, 0x52, 0x3C}},
};