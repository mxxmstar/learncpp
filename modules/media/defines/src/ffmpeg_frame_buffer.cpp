// @file ffmpeg_frame_buffer.cpp
// FFmpeg AVFrame wrapper implementation: multi-plane to packed memory
#include "defines/ffmpeg_frame_buffer.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}
#include <cstdlib>
#include <cstring>

// Iterate through AVFrame data planes, copy line by line to packed_data_ continuous memory
FFmpegFrameBuffer::FFmpegFrameBuffer(AVFrame* frame, size_t total_size)
    : frame_(frame), packed_size_(total_size) {
    if (frame_ && total_size > 0) {
        packed_data_ = static_cast<uint8_t*>(std::malloc(total_size));
        if (!packed_data_) return;
        uint8_t* dst = packed_data_;
        for (int i = 0; i < AV_NUM_DATA_POINTERS && frame->data[i]; ++i) {
            // Plane 1 (U) and 2 (V) use av_image_get_linesize to get row stride
            int row_bytes = (i == 1 || i == 2)
                ? av_image_get_linesize(static_cast<AVPixelFormat>(frame->format), frame->width, i)
                : frame->linesize[i];
            if (row_bytes <= 0) break;
            // Plane 1/2 height is half of total height (YUV420)
            int h = (i == 0) ? frame->height
                  : (i == 1 || i == 2) ? AV_CEIL_RSHIFT(frame->height, 1)
                  : 0;
            if (h <= 0) break;
            size_t plane_size = static_cast<size_t>(row_bytes) * h;
            size_t copy = (plane_size < (total_size - (dst - packed_data_)))
                          ? plane_size : (total_size - (dst - packed_data_));
            std::memcpy(dst, frame->data[i], copy);
            dst += copy;
        }
    }
}

FFmpegFrameBuffer::~FFmpegFrameBuffer() {
    std::free(packed_data_);
    if (frame_) av_frame_free(&frame_);
}

uint8_t* FFmpegFrameBuffer::Data() { return packed_data_; }
const uint8_t* FFmpegFrameBuffer::Data() const { return packed_data_; }
size_t FFmpegFrameBuffer::Size() const { return packed_size_; }
