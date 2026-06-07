/// @file ffmpeg_decoder.cpp
/// FFmpeg 软件解码器实现。

#include "decoder/ffmpeg_decoder.hpp"
#include "defines/ffmpeg_frame_buffer.hpp"

#include "common/log/logmanager.h"

extern "C" {
#include <libavutil/imgutils.h>
}

// ── ctor / dtor ────────────────────────────────────────────────────

FFmpegDecoder::~FFmpegDecoder() {
    Close();
}

// ── IDecoder ─────────────────────────────────────────────────────────

bool FFmpegDecoder::Open(const StreamInfo& info) {
    if (info.codec_type == CodecType::UNKNOWN) {
        LOG_MAIN_ERROR_AT("FFmpegDecoder:Open rejected: unknown codec");
        return false;
    }

    // 1. CodecType → AVCodecID
    AVCodecID codec_id = AV_CODEC_ID_NONE;
    switch (info.codec_type) {
        case CodecType::H264: codec_id = AV_CODEC_ID_H264; break;
        case CodecType::H265: codec_id = AV_CODEC_ID_HEVC; break;
        case CodecType::AAC:  codec_id = AV_CODEC_ID_AAC;  break;
        case CodecType::OPUS: codec_id = AV_CODEC_ID_OPUS; break;
        default:
            LOG_MAIN_ERROR_AT("FFmpegDecoder:Open: unsupported codec type {}",
                              static_cast<int>(info.codec_type));
            return false;
    }

    // 2. 查找解码器
    const AVCodec* decoder = avcodec_find_decoder(codec_id);
    if (!decoder) {
        LOG_MAIN_ERROR_AT("FFmpegDecoder:Open: avcodec_find_decoder failed for id={}",
                          static_cast<int>(codec_id));
        return false;
    }

    // 3. 分配解码器上下文
    codec_ctx_ = avcodec_alloc_context3(decoder);
    if (!codec_ctx_) {
        LOG_MAIN_ERROR_AT("FFmpegDecoder:Open: avcodec_alloc_context3 failed");
        return false;
    }

    // 4. 设置解码参数
    if (info.media_type == MediaType::VIDEO) {
        codec_ctx_->width       = info.width;
        codec_ctx_->height      = info.height;
        codec_ctx_->pix_fmt     = AV_PIX_FMT_NONE;   // 由解码器自动检测
        codec_ctx_->thread_count = 1;                  // 单线程，保证确定性
    }

    // 5. 设置 extradata（SPS/PPS 等）
    if (!info.extra_data.empty()) {
        codec_ctx_->extradata_size = static_cast<int>(info.extra_data.size());
        codec_ctx_->extradata = static_cast<uint8_t*>(
            av_malloc(info.extra_data.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!codec_ctx_->extradata) {
            LOG_MAIN_ERROR_AT("FFmpegDecoder:Open: av_malloc for extradata failed");
            avcodec_free_context(&codec_ctx_);
            return false;
        }
        std::memcpy(codec_ctx_->extradata, info.extra_data.data(),
                    info.extra_data.size());
        std::memset(codec_ctx_->extradata + info.extra_data.size(), 0,
                    AV_INPUT_BUFFER_PADDING_SIZE);
    }

    // 6. 打开解码器
    int ret = avcodec_open2(codec_ctx_, decoder, nullptr);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_MAIN_ERROR_AT("FFmpegDecoder:Open: avcodec_open2 failed: {}", buf);
        avcodec_free_context(&codec_ctx_);
        return false;
    }

    stream_info_ = info;
    LOG_MAIN_INFO_AT("FFmpegDecoder:Open success: codec={}, {}x{}",
                     static_cast<int>(info.codec_type),
                     info.width, info.height);
    return true;
}

void FFmpegDecoder::Close() {
    if (codec_ctx_) {
        // 冲刷解码器残留帧
        avcodec_send_packet(codec_ctx_, nullptr);
        (void)ReceiveFrames();

        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
    stream_info_ = {};
}

bool FFmpegDecoder::Decode(std::shared_ptr<MediaPacket> packet) {
    if (!codec_ctx_) {
        LOG_MAIN_ERROR_AT("FFmpegDecoder:Decode: codec_ctx_ is null");
        return false;
    }
    if (!packet || !packet->buffer) {
        LOG_MAIN_ERROR_AT("FFmpegDecoder:Decode: invalid packet");
        return false;
    }

    // 从 BackendHandle 获取 AVPacket*
    AVPacket* avpkt = nullptr;
    if (packet->backend.type == BackendHandle::FFMPEG) {
        avpkt = static_cast<AVPacket*>(packet->backend.ptr);
    } else {
        LOG_MAIN_ERROR_AT("FFmpegDecoder:Decode: non-FFmpeg backend not supported");
        return false;
    }

    if (!avpkt) {
        LOG_MAIN_ERROR_AT("FFmpegDecoder:Decode: backend AVPacket is null");
        return false;
    }

    // 送入解码器
    int ret = avcodec_send_packet(codec_ctx_, avpkt);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_MAIN_ERROR_AT("FFmpegDecoder:Decode: avcodec_send_packet failed: {}", buf);
        return false;
    }

    // 接收所有产生的帧
    return ReceiveFrames();
}

void FFmpegDecoder::SetFrameCallback(FrameCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    frame_cb_ = std::move(cb);
}

// ── 内部 ──────────────────────────────────────────────────────────

bool FFmpegDecoder::ReceiveFrames() {
    if (!codec_ctx_)
        return false;

    int ret = 0;
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOG_MAIN_ERROR_AT("FFmpegDecoder:ReceiveFrames: av_frame_alloc failed");
        return false;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(codec_ctx_, frame);

        if (ret == AVERROR(EAGAIN)) {
            // 解码器需要更多数据，正常
            break;
        }
        if (ret == AVERROR_EOF) {
            LOG_MAIN_DEBUG_AT("FFmpegDecoder:ReceiveFrames: EOF");
            break;
        }
        if (ret < 0) {
            char buf[AV_ERROR_MAX_STRING_SIZE];
            av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
            LOG_MAIN_ERROR_AT("FFmpegDecoder:ReceiveFrames: "
                              "avcodec_receive_frame failed: {}", buf);
            av_frame_free(&frame);
            return false;
        }

        // ── 计算 packed 大小 ──
        int size = av_image_get_buffer_size(
            static_cast<AVPixelFormat>(frame->format),
            frame->width, frame->height, 1);

        if (size <= 0) {
            LOG_MAIN_ERROR_AT("FFmpegDecoder:ReceiveFrames: "
                              "av_image_get_buffer_size failed");
            av_frame_free(&frame);
            return false;
        }

        // ── 构建 FFmpegFrameBuffer（接管 frame 所有权） ──
        auto fb = std::make_shared<FFmpegFrameBuffer>(frame, static_cast<size_t>(size));

        // 上面已经将frame的所有权转移给了fb,这里需要重新分配新 frame
        // ── 分配新 frame 用于下一轮 ──
        // TODO: 优化内存分配
        frame = av_frame_alloc();
        if (!frame) {
            LOG_MAIN_ERROR_AT("FFmpegDecoder:ReceiveFrames: "
                              "av_frame_alloc OOM after decode");
            av_frame_free(&frame);
            return false;
        }

        // ── 填充 MediaFrame ──
        auto mf = std::make_shared<MediaFrame>();
        mf->type          = MediaType::VIDEO;
        mf->pixel_format  = MapAVPixelFormat(static_cast<AVPixelFormat>(fb->GetFrame()->format));
        mf->width         = fb->GetFrame()->width;
        mf->height        = fb->GetFrame()->height;
        mf->pts           = fb->GetFrame()->pts;
        mf->dts           = fb->GetFrame()->pkt_dts;
        mf->duration      = fb->GetFrame()->duration;
        mf->keyframe      = (fb->GetFrame()->flags & AV_FRAME_FLAG_KEY) != 0;
        mf->buffer        = fb;

        mf->backend.type  = BackendHandle::FFMPEG;
        mf->backend.ptr   = fb->GetFrame();

        // ── 回调通知 ──
        FrameCallback cb;
        {
            std::lock_guard<std::mutex> lock(cb_mutex_);
            cb = frame_cb_;
        }
        if (cb) {
            cb(std::move(mf));
        }
    }

    av_frame_free(&frame);
    return true;
}

// ── 工具 ──────────────────────────────────────────────────────────

CodecType FFmpegDecoder::MapAVCodecID(AVCodecID id) {
    switch (id) {
        case AV_CODEC_ID_H264: return CodecType::H264;
        case AV_CODEC_ID_HEVC: return CodecType::H265;
        case AV_CODEC_ID_AAC:  return CodecType::AAC;
        case AV_CODEC_ID_OPUS: return CodecType::OPUS;
        default:               return CodecType::UNKNOWN;
    }
}

PixelFormat FFmpegDecoder::MapAVPixelFormat(AVPixelFormat fmt) {
    switch (fmt) {
        case AV_PIX_FMT_NV12:    return PixelFormat::kNV12;
        case AV_PIX_FMT_NV21:    return PixelFormat::kNV21;
        case AV_PIX_FMT_YUV420P: return PixelFormat::kI420;
        case AV_PIX_FMT_BGR24:   return PixelFormat::kBGR24;
        case AV_PIX_FMT_RGB24:   return PixelFormat::kRGB24;
        case AV_PIX_FMT_GRAY8:   return PixelFormat::kGRAY8;
        default:                 return PixelFormat::kUnknown;
    }
}
