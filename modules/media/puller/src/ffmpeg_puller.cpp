#include "puller/ffmpeg_puller.hpp"

#include "common/log/logmanager.h"
#include "defines/ffmpeg_packet_buffer.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

// ── ctor / dtor ────────────────────────────────────────────────────

FFmpegPuller::FFmpegPuller() {
}

FFmpegPuller::~FFmpegPuller() {
    Close();
}

// ── IPuller ─────────────────────────────────────────────────────────

bool FFmpegPuller::Open(const std::string& url) {
    // 1. 分配 FFmpeg 格式上下文
    fmt_ctx_ = avformat_alloc_context();
    if (!fmt_ctx_) {
        LOG_MAIN_ERROR_AT("avformat_alloc_context failed");
        return false;
    }

    // 2. 设置中断回调（超时控制）
    interrupt_ctx_.interrupted = false;
    interrupt_ctx_.start_time  = std::chrono::steady_clock::now();
    interrupt_ctx_.timeout_ms  = connect_timeout_ms_;

    fmt_ctx_->interrupt_callback.callback = [](void* ctx) -> int {
        auto* ic = static_cast<InterruptContext*>(ctx);
        if (ic->interrupted.load())
            return 1;
        auto now   = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - ic->start_time).count();
        if (ic->timeout_ms > 0 && elapsed > ic->timeout_ms) {
            LOG_MAIN_WARN_AT("Connection timeout after {} ms", elapsed);
            return 1;
        }
        return 0;
    };
    fmt_ctx_->interrupt_callback.opaque = &interrupt_ctx_;

    // 3. 设置传输选项
    AVDictionary* opts = nullptr;
    av_dict_set_int(&opts, "stimeout", read_timeout_ms_ * 1000, 0);
    av_dict_set_int(&opts, "timeout",  connect_timeout_ms_, 0);
    if (low_latency_) {
        av_dict_set(&opts, "fflags", "nobuffer", 0);
        av_dict_set(&opts, "flags",  "low_delay", 0);
    }
    if (!username_.empty())
        av_dict_set(&opts, "user",     username_.c_str(), 0);
    if (!password_.empty())
        av_dict_set(&opts, "password", password_.c_str(), 0);

    // 4. 打开输入
    int ret = avformat_open_input(&fmt_ctx_, url.c_str(), nullptr, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_MAIN_ERROR_AT("avformat_open_input failed: {}", buf);
        Close();
        return false;
    }

    // 5. 查找流信息
    ret = avformat_find_stream_info(fmt_ctx_, nullptr);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_MAIN_ERROR_AT("avformat_find_stream_info failed: {}", buf);
        Close();
        return false;
    }

    // 6. 选择第一个视频流
    video_stream_idx_ = -1;
    for (unsigned i = 0; i < fmt_ctx_->nb_streams; ++i) {
        if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx_ = static_cast<int>(i);
            codecpar_ = fmt_ctx_->streams[i]->codecpar;
            break;
        }
    }
    if (video_stream_idx_ < 0) {
        LOG_MAIN_ERROR_AT("no video stream found in {}", url);
        Close();
        return false;
    }

    // 7. 缓存 StreamInfo
    cached_info_.media_type   = MediaType::VIDEO;
    cached_info_.codec_type   = MapCodecID(codecpar_->codec_id);
    cached_info_.stream_index = video_stream_idx_;
    cached_info_.width        = codecpar_->width;
    cached_info_.height       = codecpar_->height;
    if (codecpar_->extradata && codecpar_->extradata_size > 0) {
        cached_info_.extra_data.assign(
            codecpar_->extradata,
            codecpar_->extradata + codecpar_->extradata_size);
    }
    cached_info_.Dump();

    return true;
}

void FFmpegPuller::Close() {
    interrupt_ctx_.interrupted = true;
    if (fmt_ctx_) {
        avformat_close_input(&fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
    codecpar_       = nullptr;
    video_stream_idx_ = -1;
    cached_info_    = {};
}

bool FFmpegPuller::ReadPacket(std::shared_ptr<MediaPacket>& packet) {
    if (fmt_ctx_ == nullptr) {
        LOG_MAIN_ERROR_AT("fmt_ctx_ is nullptr");
        return false;
    }

    // Pool-allocate a temp packet for non-video / error paths
    auto alloc_pool = [this]() -> AVPacket* {
        AVPacket* p = packet_pool_.Allocate();
        if (p) av_packet_unref(p);
        return p;
    };

    AVPacket* pkt = alloc_pool();
    if (!pkt) {
        LOG_MAIN_ERROR_AT("packet_pool_.Allocate failed");
        return false;
    }

    // 2. 读取一帧
    int ret = av_read_frame(fmt_ctx_, pkt);
    if (ret < 0) {
        av_packet_unref(pkt);
        packet_pool_.Deallocate(pkt);
        if (ret == AVERROR_EOF) {
            LOG_MAIN_DEBUG_AT("av_read_frame EOF");
            return false;
        }
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_MAIN_ERROR_AT("av_read_frame error: {}", buf);
        return false;
    }

    // 3. 只处理选中的视频流
    if (pkt->stream_index != video_stream_idx_) {
        av_packet_unref(pkt);
        packet_pool_.Deallocate(pkt);
        packet = nullptr;
        return true;
    }

    // 4. 视频包：从池转移到 av_packet_alloc 分配的包，使 FFmpegPacketBuffer
    //    析构时 av_packet_free 能正确释放 AVPacket 结构体内存。
    AVPacket* owned = av_packet_alloc();
    if (!owned) {
        av_packet_unref(pkt);
        packet_pool_.Deallocate(pkt);
        LOG_MAIN_ERROR_AT("av_packet_alloc failed");
        return false;
    }
    av_packet_move_ref(owned, pkt);  // 转移 buffer/side-data，pkt 变空
    av_packet_unref(pkt);
    packet_pool_.Deallocate(pkt);

    bool is_keyframe = (owned->flags & AV_PKT_FLAG_KEY) != 0;

    // 5. 构造 MediaPacket（零拷贝）
    auto mp = std::make_shared<MediaPacket>();
    mp->type     = MediaType::VIDEO;
    mp->codec    = MapCodecID(codecpar_->codec_id);
    mp->pts      = owned->pts;
    mp->dts      = owned->dts;
    mp->keyframe = is_keyframe;

    mp->buffer = std::make_shared<FFmpegPacketBuffer>(owned);

    mp->backend.type = BackendHandle::FFMPEG;
    mp->backend.ptr  = std::static_pointer_cast<FFmpegPacketBuffer>(mp->buffer)->GetPacket();

    packet = std::move(mp);
    return true;
}

StreamInfo FFmpegPuller::GetStreamInfo() const {
    return cached_info_;
}

void FFmpegPuller::SetEventCallback(EventCallback cb) {
    event_cb_ = std::move(cb);
}

// ── 扩展配置 ────────────────────────────────────────────────────────

void FFmpegPuller::SetConnectTimeoutMs(int ms) {
    connect_timeout_ms_ = ms;
}

void FFmpegPuller::SetReadTimeoutMs(int ms) {
    read_timeout_ms_ = ms;
}

void FFmpegPuller::SetLowLatency(bool enable) {
    low_latency_ = enable;
}

void FFmpegPuller::SetCredentials(const std::string& username,
                                  const std::string& password) {
    username_ = username;
    password_ = password;
}

// ── MapCodecID ──────────────────────────────────────────────────────

CodecType FFmpegPuller::MapCodecID(AVCodecID id) {
    switch (id) {
        case AV_CODEC_ID_H264: return CodecType::H264;
        case AV_CODEC_ID_HEVC: return CodecType::H265;
        case AV_CODEC_ID_AAC:  return CodecType::AAC;
        case AV_CODEC_ID_OPUS: return CodecType::OPUS;
        default:               return CodecType::UNKNOWN;
    }
}
