#include "puller/ffmpeg_puller.hpp"

#include "common/log/logmanager.h"
#include "defines/ffmpeg_packet_buffer.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

// ── ctor / dtor ────────────────────────────────────────────────────

FFmpegPuller::FFmpegPuller(boost::asio::io_context& io_ctx)
    : IPuller(io_ctx) {
}

FFmpegPuller::~FFmpegPuller() {
    Stop();
}

// ── MapCodecID ─────────────────────────────────────────────────────

CodecType FFmpegPuller::MapCodecID(AVCodecID id) {
    switch (id) {
        case AV_CODEC_ID_H264: return CodecType::H264;
        case AV_CODEC_ID_HEVC: return CodecType::H265;
        case AV_CODEC_ID_AAC:  return CodecType::AAC;
        case AV_CODEC_ID_OPUS: return CodecType::OPUS;
        default:               return CodecType::UNKNOWN;
    }
}

// ── OnConnect ──────────────────────────────────────────────────────

bool FFmpegPuller::OnConnect() {
    OnDisconnect();  // 清理残留

    // 1. 分配 FFmpeg 格式上下文
    fmt_ctx_ = avformat_alloc_context();
    if (!fmt_ctx_) {
        LOG_MAIN_ERROR_AT("avformat_alloc_context failed");
        return false;
    }

    // 2. 设置中断回调（用于超时控制）
    interrupt_ctx_.interrupted = false;
    interrupt_ctx_.start_time = std::chrono::steady_clock::now();
    interrupt_ctx_.timeout_ms = config_.connect_timeout_ms;

    fmt_ctx_->interrupt_callback.callback = [](void* ctx) -> int {
        auto* ic = static_cast<InterruptContext*>(ctx);
        if (ic->interrupted.load()) {
            return 1;  // 中断
        }
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - ic->start_time).count();
        if (ic->timeout_ms > 0 && elapsed > ic->timeout_ms) {
            LOG_MAIN_WARN_AT("Connection timeout after {} ms", elapsed);
            return 1;  // 超时，中断
        }
        return 0;  // 继续
    };
    fmt_ctx_->interrupt_callback.opaque = &interrupt_ctx_;

    // 3. 设置传输选项
    AVDictionary* opts = nullptr;
    /*if (config_.transport == TransportType::UDP)
        av_dict_set(&opts, "rtsp_transport", "udp", 0);
    else
        av_dict_set(&opts, "rtsp_transport", "tcp", 0);*/

    av_dict_set_int(&opts, "stimeout", config_.read_timeout_ms * 1000, 0);
    av_dict_set_int(&opts, "timeout",  config_.connect_timeout_ms, 0);
    if (config_.low_latency) {
        av_dict_set(&opts, "fflags", "nobuffer", 0);
        av_dict_set(&opts, "flags",  "low_delay", 0);
    }
    if (!config_.username.empty()) {
        av_dict_set(&opts, "user",     config_.username.c_str(), 0);
        av_dict_set(&opts, "password", config_.password.c_str(), 0);
    }

    // 4. 打开输入
    int ret = avformat_open_input(&fmt_ctx_, config_.url.c_str(), nullptr, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_MAIN_ERROR_AT("avformat_open_input failed: {}", errbuf);
        OnDisconnect();
        return false;
    }

    // 4. 查找流信息
    ret = avformat_find_stream_info(fmt_ctx_, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_MAIN_ERROR_AT("avformat_find_stream_info failed: {}", errbuf);
        OnDisconnect();
        return false;
    }

    // 5. 选择第一个视频流
    video_stream_idx_ = -1;
    for (unsigned i = 0; i < fmt_ctx_->nb_streams; ++i) {
        if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx_ = static_cast<int>(i);
            codecpar_ = fmt_ctx_->streams[i]->codecpar;
            break;
        }
    }
    if (video_stream_idx_ < 0) {
        LOG_MAIN_ERROR_AT("no video stream found");
        OnDisconnect();
        return false;
    }

    // 6. 构造并分发 StreamInfo
    StreamInfo info;
    info.media_type   = MediaType::VIDEO;
    info.codec_type   = MapCodecID(codecpar_->codec_id);
    info.stream_index = video_stream_idx_;
    info.width        = codecpar_->width;
    info.height       = codecpar_->height;
    if (codecpar_->extradata && codecpar_->extradata_size > 0) {
        info.extra_data.assign(
            codecpar_->extradata,
            codecpar_->extradata + codecpar_->extradata_size);
    }
    DispatchStreamInfo(info);

    return true;
}

// ── OnDisconnect ───────────────────────────────────────────────────

void FFmpegPuller::OnDisconnect() {
    // 设置中断标志，防止 avformat_close_input 阻塞
    interrupt_ctx_.interrupted = true;
    
    if (fmt_ctx_) {
        avformat_close_input(&fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
    codecpar_       = nullptr;
    video_stream_idx_ = -1;
}

// ── OnRead ─────────────────────────────────────────────────────────

IPuller::ReadResult FFmpegPuller::OnRead() {
    if (stopped_)
        return ReadResult::ERROR_;

    // 1. 分配 AVPacket
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        LOG_MAIN_ERROR_AT("av_packet_alloc failed");
        return IPuller::ReadResult::ERROR_;
    }
    if (fmt_ctx_ == nullptr) {
        LOG_MAIN_ERROR_AT("fmt_ctx_ is nullptr");
        return IPuller::ReadResult::ERROR_;
    }
    int ret = av_read_frame(fmt_ctx_, pkt);

    // 2. 处理读错误
    if (ret < 0) {
        av_packet_free(&pkt);
        if (ret == AVERROR_EOF)
            return ReadResult::EOF_;
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_MAIN_ERROR_AT("av_read_frame error: {}", errbuf);
        return ReadResult::ERROR_;
    }

    // 3. 只处理选中的视频流
    if (pkt->stream_index == video_stream_idx_) {
        auto mp = std::make_shared<MediaPacket>();
        mp->type     = MediaType::VIDEO;
        mp->codec    = MapCodecID(codecpar_->codec_id);
        mp->pts      = pkt->pts;
        mp->dts      = pkt->dts;
        mp->keyframe = (pkt->flags & AV_PKT_FLAG_KEY) != 0;

        // 零拷贝：FFmpegPacketBuffer 直接引用 AVPacket 数据
        mp->buffer = std::make_shared<FFmpegPacketBuffer>(av_packet_clone(pkt));

        mp->backend.type = BackendHandle::FFMPEG;
        mp->backend.ptr  = std::static_pointer_cast<FFmpegPacketBuffer>(mp->buffer)->GetPacket();

        async_bytes_received_ += pkt->size;
        async_packets_received_++;
        DispatchPacket(std::move(mp));
    }

    av_packet_free(&pkt);
    return ReadResult::OK;
}
