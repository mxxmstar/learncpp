// @file media_frame.cpp
// C API media_frame_init / media_frame_clear / media_frame_calc_video_size implementation
#include "defines/media_frame.h"
#include "defines/media_buffer.h"
#include "defines/media_frame.hpp"
#include <cstring>
#include <new>

void media_frame_init(media_frame_t* frame) {
    if (!frame) return;
    std::memset(frame, 0, sizeof(*frame));
}

void media_frame_clear(media_frame_t* frame) {
    if (!frame) return;
    if (frame->buffer) {
        media_buffer_destroy(frame->buffer);
    }
    media_frame_init(frame);
}

int32_t media_frame_calc_video_size(int32_t w, int32_t h, pixel_format_t pix_fmt) {
    // Simplified calculation for common formats
    switch (pix_fmt) {
        case pixel_format_t::PIXEL_FORMAT_YUV420P:
            return w * h * 3 / 2;
        case pixel_format_t::PIXEL_FORMAT_NV12:
            return w * h * 3 / 2;
        case pixel_format_t::PIXEL_FORMAT_RGB24:
        case pixel_format_t::PIXEL_FORMAT_BGR24:
            return w * h * 3;
        default:
            return w * h * 3;  // Default to RGB size
    }
}
