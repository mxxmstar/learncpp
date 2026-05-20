#pragma once
/// @file ffmpeg_frame_buffer.hpp
/// FFmpeg AVFrame 包装器，将多平面 AVFrame pack 为连续内存后适配 IMediaBuffer 接口。

#include "i_media_buffer.hpp"

struct AVFrame;

/// 包装 FFmpeg AVFrame 的 IMediaBuffer 实现
/// 注意：构造函数会将 AVFrame 的多平面数据 pack 为一块连续内存
class FFmpegFrameBuffer : public IMediaBuffer {
public:
    /// 接管 frame 所有权，将各平面数据复制到 packed_data_ 连续内存
    explicit FFmpegFrameBuffer(AVFrame* frame, size_t total_size);
    ~FFmpegFrameBuffer() override;
    uint8_t* Data() override;
    const uint8_t* Data() const override;
    size_t Size() const override;

    /// 获取内部的原始 AVFrame 指针
    AVFrame* GetFrame() const { return frame_; }

private:
    AVFrame* frame_{nullptr};         ///< 被包装的 AVFrame（析构时释放）
    uint8_t* packed_data_{nullptr};   ///< 连续 packed 后的数据
    size_t   packed_size_{0};         ///< packed 数据总大小
};
