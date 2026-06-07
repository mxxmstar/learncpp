// FFmpegEncoder: 基于 FFmpeg 的编码器实现
// 继承 IEncoder 接口，封装 avcodec 的编码流程

#pragma once

#include "encoder/i_encoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
}

class FFmpegEncoder : public IEncoder {
public:
    FFmpegEncoder() = default;
    ~FFmpegEncoder() override;

    FFmpegEncoder(const FFmpegEncoder&) = delete;
    FFmpegEncoder& operator=(const FFmpegEncoder&) = delete;

    bool Open(const EncoderConfig& cfg) override;
    bool Encode(FramePtr frame, std::vector<PacketPtr>& packets) override;
    void Close() override;

    // 编码类型与 FFmpeg 类型之间的转换函数
    static AVCodecID MapCodecType(CodecType type);
    static AVPixelFormat MapPixelFormat(PixelFormat fmt);
    static CodecType MapAVCodecID(AVCodecID id);
    static PixelFormat MapAVPixelFormat(AVPixelFormat fmt);

private:
    // 从编码器中循环取出所有已编码包
    bool ReceivePackets(std::vector<PacketPtr>& packets);
    // 根据 codec_id、输入像素格式和可选的编码器名称，查找合适的编码器
    const AVCodec* FindEncoder(AVCodecID codec_id, AVPixelFormat input_fmt,
                               const std::string& encoder_name,
                               AVPixelFormat& encoder_fmt) const;
    // 检查编码器是否支持指定像素格式
    bool IsPixelFormatSupported(const AVCodec* codec, AVPixelFormat fmt) const;
    // 构造输入 AVFrame，支持 FFmpeg 后端和非 FFmpeg 两种输入源
    AVFrame* BuildInputFrame(const FramePtr& frame);
    // 将一个 AVFrame 拷贝到另一个 AVFrame（支持格式转换）
    bool CopyAVFrameToAVFrame(const AVFrame* src, AVFrame* dst) const;
    // 将 MediaFrame 的数据拷贝到 AVFrame
    bool CopyPackedFrameToAVFrame(const MediaFrame& src, AVFrame* dst) const;
    // 解析帧的 PTS，若帧的 pts 为 0 则自动分配递增 PTS
    int64_t ResolveFramePts(const MediaFrame& frame);

    AVCodecContext* codec_ctx_{nullptr};  // FFmpeg 编码器上下文
    EncoderConfig config_;                // 当前编码器配置
    AVPixelFormat input_pix_fmt_{AV_PIX_FMT_NONE};   // 输入像素格式
    AVPixelFormat encoder_pix_fmt_{AV_PIX_FMT_NONE};  // 编码器实际使用的像素格式
    int64_t next_pts_{0};                // 自动递增的 PTS 计数器
};
