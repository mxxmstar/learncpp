#pragma once
/// @file ffmpeg_packet_buffer.hpp
/// FFmpeg AVPacket 包装器，将 AVPacket 适配为 IMediaBuffer 接口。

#include "i_media_buffer.hpp"

struct AVPacket;

/// 包装 FFmpeg AVPacket 的 IMediaBuffer 实现
class FFmpegPacketBuffer : public IMediaBuffer {
public:
    /// 接管 pkt 所有权，析构时调用 av_packet_free
    explicit FFmpegPacketBuffer(AVPacket* pkt);
    ~FFmpegPacketBuffer() override;

    // 禁止拷贝
    FFmpegPacketBuffer(const FFmpegPacketBuffer&) = delete;
    FFmpegPacketBuffer& operator=(const FFmpegPacketBuffer&) = delete;

    FFmpegPacketBuffer(FFmpegPacketBuffer&& other) noexcept;
    FFmpegPacketBuffer& operator=(FFmpegPacketBuffer&& other) noexcept;
    
    uint8_t* Data() override;
    const uint8_t* Data() const override;
    size_t Size() const override;

    /// 获取内部的原始 AVPacket 指针
    AVPacket* GetPacket() const { return pkt_; }

private:
    AVPacket* pkt_{nullptr};  ///< 被包装的 AVPacket
};
