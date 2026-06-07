#include "pusher/ffmpeg_protocol_adapter.hpp"

#include "common/log/logmanager.h"

#include <cstring>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

namespace {

std::string ErrorString(int err) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_make_error_string(buf, sizeof(buf), err);
    return buf;
}

bool StartsWithRtsp(const std::string& url) {
    return url.rfind("rtsp://", 0) == 0 || url.rfind("rtsps://", 0) == 0;
}

} // namespace

FFmpegProtocolAdapter::~FFmpegProtocolAdapter() {
    Close();
}

bool FFmpegProtocolAdapter::Connect(const PusherConfig& config) {
    Close();

    if (!config.IsValid()) {
        LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: invalid pusher config");
        return false;
    }

    config_ = config;

    const char* format_name = config_.format_name.empty()
        ? nullptr
        : config_.format_name.c_str();

    int ret = avformat_alloc_output_context2(
        &fmt_ctx_, nullptr, format_name, config_.url.c_str());
    if (ret < 0 || !fmt_ctx_) {
        LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: alloc output context failed: {}",
                          ErrorString(ret));
        FreeContext(false);
        return false;
    }

    if (!BuildStream()) {
        FreeContext(false);
        return false;
    }

    AVDictionary* opts = nullptr;
    if (StartsWithRtsp(config_.url) && !config_.rtsp_transport.empty()) {
        av_dict_set(&opts, "rtsp_transport", config_.rtsp_transport.c_str(), 0);
    }

    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open2(&fmt_ctx_->pb, config_.url.c_str(),
                         AVIO_FLAG_WRITE, nullptr, &opts);
        if (ret < 0) {
            LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: open output url failed: {}",
                              ErrorString(ret));
            av_dict_free(&opts);
            FreeContext(false);
            return false;
        }
    }

    ret = avformat_write_header(fmt_ctx_, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: write header failed: {}",
                          ErrorString(ret));
        FreeContext(false);
        return false;
    }

    header_written_ = true;
    LOG_MAIN_INFO_AT("FFmpegProtocolAdapter: connected to {}", config_.url);
    return true;
}

bool FFmpegProtocolAdapter::Send(const MediaPacket& pkt) {
    if (!fmt_ctx_ || stream_index_ < 0 || !header_written_) {
        LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: Send called before Connect");
        return false;
    }

    if (pkt.type != MediaType::UNKNOWN && pkt.type != config_.media_type) {
        LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: media type mismatch");
        return false;
    }

    if (pkt.codec != CodecType::UNKNOWN && pkt.codec != config_.codec_type) {
        LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: codec mismatch");
        return false;
    }

    AVPacket* out = av_packet_alloc();
    if (!out) {
        LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: av_packet_alloc failed");
        return false;
    }

    if (!BuildPacket(pkt, out)) {
        av_packet_free(&out);
        return false;
    }

    const int ret = av_write_frame(fmt_ctx_, out);
    av_packet_free(&out);
    if (ret < 0) {
        LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: write packet failed: {}",
                          ErrorString(ret));
        return false;
    }

    return true;
}

bool FFmpegProtocolAdapter::Close() {
    return FreeContext(true);
}

int FFmpegProtocolAdapter::MapCodecType(CodecType type) {
    switch (type) {
        case CodecType::H264:
            return AV_CODEC_ID_H264;
        case CodecType::H265:
            return AV_CODEC_ID_HEVC;
        case CodecType::AAC:
            return AV_CODEC_ID_AAC;
        case CodecType::OPUS:
            return AV_CODEC_ID_OPUS;
        default:
            return AV_CODEC_ID_NONE;
    }
}

bool FFmpegProtocolAdapter::BuildStream() {
    AVStream* stream = avformat_new_stream(fmt_ctx_, nullptr);
    if (!stream) {
        LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: avformat_new_stream failed");
        return false;
    }

    stream_index_ = stream->index;
    stream->time_base = AVRational{config_.time_base_num, config_.time_base_den};

    AVCodecParameters* par = stream->codecpar;
    par->codec_id = static_cast<AVCodecID>(MapCodecType(config_.codec_type));
    par->codec_tag = 0;

    if (par->codec_id == AV_CODEC_ID_NONE) {
        LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: unsupported codec {}",
                          static_cast<int>(config_.codec_type));
        return false;
    }

    if (config_.media_type == MediaType::VIDEO) {
        par->codec_type = AVMEDIA_TYPE_VIDEO;
        par->width = config_.width;
        par->height = config_.height;
    } else if (config_.media_type == MediaType::AUDIO) {
        par->codec_type = AVMEDIA_TYPE_AUDIO;
        par->sample_rate = config_.sample_rate;
#if LIBAVUTIL_VERSION_MAJOR >= 57
        av_channel_layout_uninit(&par->ch_layout);
        av_channel_layout_default(&par->ch_layout, config_.channels);
#else
        par->channels = config_.channels;
        par->channel_layout = av_get_default_channel_layout(config_.channels);
#endif
    } else {
        LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: unsupported media type {}",
                          static_cast<int>(config_.media_type));
        return false;
    }

    if (!config_.extra_data.empty()) {
        const size_t size = config_.extra_data.size();
        par->extradata = static_cast<uint8_t*>(
            av_mallocz(size + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!par->extradata) {
            LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: allocate extradata failed");
            return false;
        }
        std::memcpy(par->extradata, config_.extra_data.data(), size);
        par->extradata_size = static_cast<int>(size);
    }

    return true;
}

bool FFmpegProtocolAdapter::BuildPacket(const MediaPacket& pkt, AVPacket* out) const {
    if (!out) {
        return false;
    }

    av_packet_unref(out);

    int ret = 0;
    if (pkt.backend.type == BackendHandle::FFMPEG && pkt.backend.ptr) {
        auto* src = static_cast<AVPacket*>(pkt.backend.ptr);
        ret = av_packet_ref(out, src);
        if (ret < 0) {
            LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: packet ref failed: {}",
                              ErrorString(ret));
            return false;
        }
    } else {
        if (!pkt.buffer || !pkt.buffer->Data() || pkt.buffer->Size() == 0) {
            LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: packet buffer is empty");
            return false;
        }

        const auto size = static_cast<int>(pkt.buffer->Size());
        ret = av_new_packet(out, size);
        if (ret < 0) {
            LOG_MAIN_ERROR_AT("FFmpegProtocolAdapter: packet allocation failed: {}",
                              ErrorString(ret));
            return false;
        }
        std::memcpy(out->data, pkt.buffer->Data(), pkt.buffer->Size());
    }

    out->stream_index = stream_index_;
    out->pts = pkt.pts;
    out->dts = pkt.dts;
    out->pos = -1;

    if (pkt.keyframe) {
        out->flags |= AV_PKT_FLAG_KEY;
    } else {
        out->flags &= ~AV_PKT_FLAG_KEY;
    }

    return true;
}

bool FFmpegProtocolAdapter::FreeContext(bool write_trailer) {
    bool ok = true;

    if (fmt_ctx_ && write_trailer && header_written_) {
        const int ret = av_write_trailer(fmt_ctx_);
        if (ret < 0) {
            LOG_MAIN_WARN_AT("FFmpegProtocolAdapter: write trailer failed: {}",
                             ErrorString(ret));
            ok = false;
        }
    }

    if (fmt_ctx_) {
        if (fmt_ctx_->pb && !(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
            const int ret = avio_closep(&fmt_ctx_->pb);
            if (ret < 0) {
                LOG_MAIN_WARN_AT("FFmpegProtocolAdapter: close output failed: {}",
                                 ErrorString(ret));
                ok = false;
            }
        }

        avformat_free_context(fmt_ctx_);
        fmt_ctx_ = nullptr;
    }

    stream_index_ = -1;
    header_written_ = false;
    config_ = {};

    return ok;
}
