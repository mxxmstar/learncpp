// @file media_frame.cpp
// C API media_frame_init / media_frame_clear / media_frame_calc_video_size 实现�?
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
    // 先释放内部的 buffer，再清零整个结构�?    media_buffer_destroy(frame->buffer);
    frame->buffer = nullptr;
    std::memset(frame, 0, sizeof(*frame));
}

int32_t media_frame_calc_video_size(int32_t w, int32_t h, pixel_format_t fmt) {
    return MediaFrame::CalcVideoSize(w, h, static_cast<PixelFormat>(fmt));
}
