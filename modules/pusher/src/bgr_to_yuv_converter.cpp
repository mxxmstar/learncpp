#include "pusher/bgr_to_yuv_converter.h"
#include "common/log/logmanager.h"
#include <cstring>

bool BgrToYuvConverter::Initialize(int width, int height) {
    if (width <= 0 || height <= 0) {
        LOG_MAIN_ERROR_AT("BgrToYuvConverter: Invalid dimensions {}x{}", width, height);
        return false;
    }
    
    width_ = width;
    height_ = height;
    yuv_size_ = width * height * 3 / 2;
    buffer_.resize(yuv_size_);
    
    LOG_MAIN_INFO_AT("BgrToYuvConverter initialized: {}x{}, size={}", width, height, yuv_size_);
    return true;
}

void BgrToYuvConverter::Convert(const cv::Mat& bgr, uint8_t* yuv_out) {
    if (!yuv_out || bgr.empty()) return;
    
    const uint8_t* bgr_data = bgr.data;
    int w = bgr.cols;
    int h = bgr.rows;
    
    uint8_t* y_plane = yuv_out;
    uint8_t* u_plane = yuv_out + w * h;
    uint8_t* v_plane = yuv_out + w * h + (w * h / 4);
    
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            int idx = row * w + col;
            uint8_t b = bgr_data[idx * 3];
            uint8_t g = bgr_data[idx * 3 + 1];
            uint8_t r = bgr_data[idx * 3 + 2];
            
            uint8_t y = static_cast<uint8_t>((66 * r + 129 * g + 25 * b + 128) >> 8);
            y_plane[idx] = (y < 16) ? 16 : (y > 235 ? 235 : y);
            
            if (row % 2 == 0 && col % 2 == 0) {
                int uv_idx = (row / 2) * (w / 2) + (col / 2);
                uint8_t u = static_cast<uint8_t>((-38 * r - 74 * g + 112 * b + 128) >> 8);
                uint8_t v = static_cast<uint8_t>((112 * r - 94 * g - 18 * b + 128) >> 8);
                u_plane[uv_idx] = (u < 16) ? 16 : (u > 240 ? 240 : u);
                v_plane[uv_idx] = (v < 16) ? 16 : (v > 240 ? 240 : v);
            }
        }
    }
}