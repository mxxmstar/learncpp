// FFmpegEncoder 实现: 基于 FFmpeg libavcodec 的编解码器封装
// 支持 H264/H265 视频编码，以及多种像素格式的输入和自动转换

#include "encoder/ffmpeg_encoder.hpp"

#include "common/log/logmanager.h"
#include "defines/ffmpeg_packet_buffer.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

extern "C" {
#include <libavutil/dict.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}

FFmpegEncoder::~FFmpegEncoder() {
    Close();
}

// 打开编码器，配置所有编码参数，初始化 FFmpeg 编码上下文
bool FFmpegEncoder::Open(const EncoderConfig& cfg) {
    Close();

    // 参数校验
    if (cfg.media_type == MediaType::AUDIO) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:Open: audio encoding is not supported yet");
        return false;
    }
    if (cfg.width <= 0 || cfg.height <= 0) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:Open: invalid size {}x{}", cfg.width, cfg.height);
        return false;
    }
    if (cfg.fps_num <= 0 || cfg.fps_den <= 0) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:Open: invalid fps {}/{}", cfg.fps_num, cfg.fps_den);
        return false;
    }

    // 转换编码器类型和像素格式
    const AVCodecID codec_id = MapCodecType(cfg.codec_type);
    if (codec_id == AV_CODEC_ID_NONE) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:Open: unsupported codec type {}",
                          static_cast<int>(cfg.codec_type));
        return false;
    }

    const AVPixelFormat pix_fmt = MapPixelFormat(cfg.pixel_format);
    if (pix_fmt == AV_PIX_FMT_NONE) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:Open: unsupported pixel format {}",
                          static_cast<int>(cfg.pixel_format));
        return false;
    }

    // 查找合适的编码器
    AVPixelFormat encoder_pix_fmt = AV_PIX_FMT_NONE;
    const AVCodec* codec = FindEncoder(codec_id, pix_fmt, cfg.encoder_name, encoder_pix_fmt);
    if (!codec) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:Open: encoder not found for codec={}, name={}, fmt={}",
                          static_cast<int>(codec_id), cfg.encoder_name,
                          static_cast<int>(pix_fmt));
        return false;
    }

    // 分配编码器上下文
    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:Open: avcodec_alloc_context3 failed");
        return false;
    }

    // 设置编码参数
    codec_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx_->codec_id = codec_id;
    codec_ctx_->width = cfg.width;
    codec_ctx_->height = cfg.height;
    codec_ctx_->pix_fmt = encoder_pix_fmt;
    codec_ctx_->time_base = AVRational{
        cfg.time_base_num > 0 ? cfg.time_base_num : cfg.fps_den,
        cfg.time_base_den > 0 ? cfg.time_base_den : cfg.fps_num
    };
    codec_ctx_->framerate = AVRational{cfg.fps_num, cfg.fps_den};
    codec_ctx_->bit_rate = cfg.bitrate;
    codec_ctx_->gop_size = cfg.gop_size;
    codec_ctx_->max_b_frames = cfg.max_b_frames;
    codec_ctx_->thread_count = 1;
    if (cfg.global_header) {
        codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 设置编码器选项字典（preset/tune/crf）
    AVDictionary* opts = nullptr;
    if (!cfg.preset.empty()) {
        av_dict_set(&opts, "preset", cfg.preset.c_str(), 0);
    }
    if (!cfg.tune.empty()) {
        av_dict_set(&opts, "tune", cfg.tune.c_str(), 0);
    }
    if (cfg.crf >= 0) {
        av_dict_set_int(&opts, "crf", cfg.crf, 0);
    }

    // 打开编码器
    const int ret = avcodec_open2(codec_ctx_, codec, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_MAIN_ERROR_AT("FFmpegEncoder:Open: avcodec_open2 failed: {}", buf);
        avcodec_free_context(&codec_ctx_);
        return false;
    }

    config_ = cfg;
    input_pix_fmt_ = pix_fmt;
    encoder_pix_fmt_ = encoder_pix_fmt;
    next_pts_ = 0;
    LOG_MAIN_INFO_AT("FFmpegEncoder:Open success: codec={}, encoder={}, {}x{}, fps={}/{}, input_fmt={}, encoder_fmt={}",
                     static_cast<int>(cfg.codec_type),
                     codec->name ? codec->name : "<unknown>",
                     cfg.width, cfg.height, cfg.fps_num, cfg.fps_den,
                     static_cast<int>(input_pix_fmt_),
                     static_cast<int>(encoder_pix_fmt_));
    return true;
}

// 编码一帧: 将 MediaFrame 送入编码器，取出所有已编码的 Packet
// frame 为 nullptr 时表示刷新编码器缓存
bool FFmpegEncoder::Encode(FramePtr frame, std::vector<PacketPtr>& packets) {
    if (!codec_ctx_) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:Encode: codec_ctx_ is null");
        return false;
    }

    // flush 模式: 送入 nullptr 触发编码器输出剩余帧
    if (!frame) {
        const int ret = avcodec_send_frame(codec_ctx_, nullptr);
        if (ret < 0 && ret != AVERROR_EOF) {
            char buf[AV_ERROR_MAX_STRING_SIZE];
            av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
            LOG_MAIN_ERROR_AT("FFmpegEncoder:Encode: flush failed: {}", buf);
            return false;
        }
        return ReceivePackets(packets);
    }

    // 校验帧尺寸和像素格式
    const int frame_width = frame->width > 0 ? frame->width : config_.width;
    const int frame_height = frame->height > 0 ? frame->height : config_.height;
    if (frame_width != config_.width || frame_height != config_.height) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:Encode: frame size mismatch {}x{}, expected {}x{}",
                          frame_width, frame_height, config_.width, config_.height);
        return false;
    }
    if (frame->pixel_format != PixelFormat::kUnknown &&
        frame->pixel_format != config_.pixel_format) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:Encode: pixel format mismatch {}, expected {}",
                          static_cast<int>(frame->pixel_format),
                          static_cast<int>(config_.pixel_format));
        return false;
    }

    // 构造编码器输入的 AVFrame
    AVFrame* input = BuildInputFrame(frame);
    if (!input) {
        return false;
    }

    // 发送帧到编码器，若编码器输出队列满（EAGAIN）则先取包再重试
    int ret = avcodec_send_frame(codec_ctx_, input);
    if (ret == AVERROR(EAGAIN)) {
        if (!ReceivePackets(packets)) {
            av_frame_free(&input);
            return false;
        }
        ret = avcodec_send_frame(codec_ctx_, input);
    }
    av_frame_free(&input);

    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_MAIN_ERROR_AT("FFmpegEncoder:Encode: avcodec_send_frame failed: {}", buf);
        return false;
    }

    return ReceivePackets(packets);
}

// 关闭编码器，释放所有资源
void FFmpegEncoder::Close() {
    if (codec_ctx_) {
        // flush 编码器并丢弃剩余包
        std::vector<PacketPtr> ignored;
        (void)avcodec_send_frame(codec_ctx_, nullptr);
        (void)ReceivePackets(ignored);
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
    config_ = {};
    input_pix_fmt_ = AV_PIX_FMT_NONE;
    encoder_pix_fmt_ = AV_PIX_FMT_NONE;
    next_pts_ = 0;
}

// 从编码器循环取包，包装为 MediaPacket 后追加到输出列表
bool FFmpegEncoder::ReceivePackets(std::vector<PacketPtr>& packets) {
    if (!codec_ctx_) {
        return false;
    }

    while (true) {
        // T
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
            LOG_MAIN_ERROR_AT("FFmpegEncoder:ReceivePackets: av_packet_alloc failed");
            return false;
        }

        const int ret = avcodec_receive_packet(codec_ctx_, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            return true;
        }
        if (ret < 0) {
            char buf[AV_ERROR_MAX_STRING_SIZE];
            av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
            LOG_MAIN_ERROR_AT("FFmpegEncoder:ReceivePackets: avcodec_receive_packet failed: {}",
                              buf);
            av_packet_free(&pkt);
            return false;
        }

        // 包装为 MediaPacket，保持 FFmpeg 后端引用
        auto pkt_buffer = std::make_shared<FFmpegPacketBuffer>(pkt);
        auto media_packet = std::make_shared<MediaPacket>();
        media_packet->type = MediaType::VIDEO;
        media_packet->codec = config_.codec_type;
        media_packet->pts = pkt->pts;
        media_packet->dts = pkt->dts;
        media_packet->keyframe = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        media_packet->buffer = pkt_buffer;
        media_packet->backend.type = BackendHandle::FFMPEG;
        media_packet->backend.ptr = pkt_buffer->GetPacket();

        packets.emplace_back(std::move(media_packet));
    }
}

// 查找适配的编码器: 优先按名称查找，否则遍历所有编码器
// 支持输入 YUV420P 时回退到 NV12（某些编码器只支持 NV12）
const AVCodec* FFmpegEncoder::FindEncoder(AVCodecID codec_id, AVPixelFormat input_fmt,
                                          const std::string& encoder_name,
                                          AVPixelFormat& encoder_fmt) const {

    // 检查编码器是否支持指定像素格式
    auto accepts = [&](const AVCodec* codec, AVPixelFormat fmt) {
        return codec && codec->id == codec_id && av_codec_is_encoder(codec) &&
               IsPixelFormatSupported(codec, fmt);
    };

    // 按名称查找
    if (!encoder_name.empty()) {
        const AVCodec* named = avcodec_find_encoder_by_name(encoder_name.c_str());
        if (!named || named->id != codec_id || !av_codec_is_encoder(named)) {
            return nullptr;
        }
        if (IsPixelFormatSupported(named, input_fmt)) {
            encoder_fmt = input_fmt;
            return named;
        }
        // 输入 YUV420P 时尝试 NV12 作为备选
        if (input_fmt == AV_PIX_FMT_YUV420P && IsPixelFormatSupported(named, AV_PIX_FMT_NV12)) {
            encoder_fmt = AV_PIX_FMT_NV12;
            return named;
        }
        return nullptr;
    }

    // 遍历所有编码器
    const AVCodec* fallback = nullptr;
    void* iter = nullptr;
    while (const AVCodec* codec = av_codec_iterate(&iter)) {
        if (accepts(codec, input_fmt)) {
            encoder_fmt = input_fmt;
            return codec;
        }
        // YUV420P -> NV12 回退
        if (!fallback && input_fmt == AV_PIX_FMT_YUV420P &&
            accepts(codec, AV_PIX_FMT_NV12)) {
            fallback = codec;
        }
    }

    if (fallback) {
        encoder_fmt = AV_PIX_FMT_NV12;
        return fallback;
    }

    return nullptr;
}

// 检查编码器是否支持指定像素格式（兼容 FFmpeg 不同版本）
bool FFmpegEncoder::IsPixelFormatSupported(const AVCodec* codec, AVPixelFormat fmt) const {
    if (!codec) {
        return false;
    }

#if LIBAVCODEC_VERSION_MAJOR >= 61
    // FFmpeg 6.1+ 使用 avcodec_get_supported_config API
    const void* configs = nullptr;
    int config_count = 0;
    const int ret = avcodec_get_supported_config(
        nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &configs, &config_count);
    if (ret < 0 || !configs || config_count <= 0) {
        return true;
    }

    const auto* pix_fmts = static_cast<const AVPixelFormat*>(configs);
    for (int i = 0; i < config_count; ++i) {
        if (pix_fmts[i] == fmt) {
            return true;
        }
    }
    return false;
#else
    // 旧版本直接遍历 codec->pix_fmts 数组
    if (!codec->pix_fmts) {
        return true;
    }
    for (const AVPixelFormat* p = codec->pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == fmt) {
            return true;
        }
    }
    return false;
#endif
}

// 构建编码器输入的 AVFrame
// 支持两种输入源: FFmpeg 后端（AVFrame）和 MediaFrame 数据
AVFrame* FFmpegEncoder::BuildInputFrame(const FramePtr& frame) {
    // 输入源为 FFmpeg 后端 AVFrame
    if (frame->backend.type == BackendHandle::FFMPEG && frame->backend.ptr) {
        auto* src = static_cast<AVFrame*>(frame->backend.ptr);
        if (!src) {
            LOG_MAIN_ERROR_AT("FFmpegEncoder:BuildInputFrame: backend AVFrame is null");
            return nullptr;
        }
        if (src->width != config_.width || src->height != config_.height) {
            LOG_MAIN_ERROR_AT("FFmpegEncoder:BuildInputFrame: backend frame mismatch");
            return nullptr;
        }
        // 像素格式匹配时直接 clone
        if (src->format == codec_ctx_->pix_fmt) {
            AVFrame* cloned = av_frame_clone(src);
            if (!cloned) {
                LOG_MAIN_ERROR_AT("FFmpegEncoder:BuildInputFrame: av_frame_clone failed");
                return nullptr;
            }
            cloned->pts = ResolveFramePts(*frame);
            return cloned;
        }

        // 格式不匹配时创建新帧并转换
        AVFrame* converted = av_frame_alloc();
        if (!converted) {
            LOG_MAIN_ERROR_AT("FFmpegEncoder:BuildInputFrame: av_frame_alloc failed");
            return nullptr;
        }
        converted->format = codec_ctx_->pix_fmt;
        converted->width = config_.width;
        converted->height = config_.height;
        converted->pts = ResolveFramePts(*frame);

        int ret = av_frame_get_buffer(converted, 32);
        if (ret < 0) {
            char buf[AV_ERROR_MAX_STRING_SIZE];
            av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
            LOG_MAIN_ERROR_AT("FFmpegEncoder:BuildInputFrame: av_frame_get_buffer failed: {}",
                              buf);
            av_frame_free(&converted);
            return nullptr;
        }
        ret = av_frame_make_writable(converted);
        if (ret < 0) {
            char buf[AV_ERROR_MAX_STRING_SIZE];
            av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
            LOG_MAIN_ERROR_AT("FFmpegEncoder:BuildInputFrame: av_frame_make_writable failed: {}",
                              buf);
            av_frame_free(&converted);
            return nullptr;
        }
        if (!CopyAVFrameToAVFrame(src, converted)) {
            av_frame_free(&converted);
            return nullptr;
        }
        return converted;
    }

    // 输入源为 MediaFrame（buffer 数据）
    if (!frame->buffer || !frame->buffer->Data()) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:BuildInputFrame: frame buffer is null");
        return nullptr;
    }

    AVFrame* dst = av_frame_alloc();
    if (!dst) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:BuildInputFrame: av_frame_alloc failed");
        return nullptr;
    }

    dst->format = codec_ctx_->pix_fmt;
    dst->width = config_.width;
    dst->height = config_.height;
    dst->pts = ResolveFramePts(*frame);

    int ret = av_frame_get_buffer(dst, 32);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_MAIN_ERROR_AT("FFmpegEncoder:BuildInputFrame: av_frame_get_buffer failed: {}",
                          buf);
        av_frame_free(&dst);
        return nullptr;
    }

    ret = av_frame_make_writable(dst);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_MAIN_ERROR_AT("FFmpegEncoder:BuildInputFrame: av_frame_make_writable failed: {}",
                          buf);
        av_frame_free(&dst);
        return nullptr;
    }

    if (!CopyPackedFrameToAVFrame(*frame, dst)) {
        av_frame_free(&dst);
        return nullptr;
    }

    return dst;
}

// AVFrame 间拷贝，支持 YUV420P <-> NV12 格式转换
bool FFmpegEncoder::CopyAVFrameToAVFrame(const AVFrame* src, AVFrame* dst) const {
    if (!src || !dst) {
        return false;
    }

    // 格式相同时使用 av_image_copy 直接拷贝
    if (src->format == dst->format) {
        const uint8_t* src_data[4] = {
            src->data[0], src->data[1], src->data[2], src->data[3]
        };
        int src_linesize[4] = {
            src->linesize[0], src->linesize[1], src->linesize[2], src->linesize[3]
        };
        av_image_copy(dst->data, dst->linesize,
                      src_data, src_linesize,
                      static_cast<AVPixelFormat>(src->format), src->width, src->height);
        return true;
    }

    // YUV420P -> NV12 格式转换
    if (src->format == AV_PIX_FMT_YUV420P && dst->format == AV_PIX_FMT_NV12) {
        const int uv_width = (src->width + 1) / 2;
        const int uv_height = (src->height + 1) / 2;
        if (!src->data[0] || !src->data[1] || !src->data[2] ||
            !dst->data[0] || !dst->data[1]) {
            LOG_MAIN_ERROR_AT("FFmpegEncoder:CopyAVFrameToAVFrame: invalid I420/NV12 planes");
            return false;
        }

        // 复制 Y 平面
        for (int row = 0; row < src->height; ++row) {
            std::memcpy(dst->data[0] + static_cast<size_t>(row) * dst->linesize[0],
                        src->data[0] + static_cast<size_t>(row) * src->linesize[0],
                        static_cast<size_t>(src->width));
        }
        // U/V 交错合并为 NV12
        for (int row = 0; row < uv_height; ++row) {
            const uint8_t* u = src->data[1] + static_cast<size_t>(row) * src->linesize[1];
            const uint8_t* v = src->data[2] + static_cast<size_t>(row) * src->linesize[2];
            uint8_t* uv = dst->data[1] + static_cast<size_t>(row) * dst->linesize[1];
            for (int col = 0; col < uv_width; ++col) {
                uv[col * 2] = u[col];
                uv[col * 2 + 1] = v[col];
            }
        }
        return true;
    }

    LOG_MAIN_ERROR_AT("FFmpegEncoder:CopyAVFrameToAVFrame: unsupported conversion {} -> {}",
                      src->format, dst->format);
    return false;
}

// 将 MediaFrame（packed 格式）的数据拷贝到 AVFrame
// 支持 I420/NV12/NV21/BGR24/RGB24/GRAY8 等格式到编码器所需格式的转换
bool FFmpegEncoder::CopyPackedFrameToAVFrame(const MediaFrame& src, AVFrame* dst) const {
    const uint8_t* base = src.buffer ? src.buffer->Data() : nullptr;
    const size_t buffer_size = src.buffer ? src.buffer->Size() : 0;
    if (!base || buffer_size == 0 || !dst) {
        LOG_MAIN_ERROR_AT("FFmpegEncoder:CopyPackedFrameToAVFrame: invalid buffer");
        return false;
    }

    const int width = config_.width;
    const int height = config_.height;
    const PixelFormat fmt = src.pixel_format == PixelFormat::kUnknown
        ? config_.pixel_format
        : src.pixel_format;

    // lambdas 辅助函数
    auto source_stride = [&](int plane, int fallback) {
        return src.stride[plane] > 0 ? src.stride[plane] : fallback;
    };
    auto source_offset = [&](int plane, size_t fallback) {
        if (plane == 0) {
            return src.plane_offset[0] > 0 ? static_cast<size_t>(src.plane_offset[0]) : size_t{0};
        }
        return src.plane_offset[plane] > 0
            ? static_cast<size_t>(src.plane_offset[plane])
            : fallback;
    };
    auto copy_plane = [&](int plane, size_t offset, int src_stride,
                          int row_bytes, int rows) -> bool {
        if (rows <= 0 || row_bytes <= 0 || src_stride < row_bytes || !dst->data[plane]) {
            LOG_MAIN_ERROR_AT("FFmpegEncoder:CopyPackedFrameToAVFrame: invalid plane {}",
                              plane);
            return false;
        }
        const size_t last_byte = offset +
            static_cast<size_t>(src_stride) * static_cast<size_t>(rows - 1) +
            static_cast<size_t>(row_bytes);
        if (last_byte > buffer_size) {
            LOG_MAIN_ERROR_AT("FFmpegEncoder:CopyPackedFrameToAVFrame: plane {} exceeds buffer",
                              plane);
            return false;
        }

        const uint8_t* src_plane = base + offset;
        uint8_t* dst_plane = dst->data[plane];
        for (int row = 0; row < rows; ++row) {
            std::memcpy(dst_plane + static_cast<size_t>(row) * dst->linesize[plane],
                        src_plane + static_cast<size_t>(row) * src_stride,
                        static_cast<size_t>(row_bytes));
        }
        return true;
    };
    auto interleave_i420_to_nv12 = [&](size_t u_offset, int u_stride,
                                       size_t v_offset, int v_stride,
                                       int uv_width, int uv_height) -> bool {
        if (!dst->data[1] || u_stride < uv_width || v_stride < uv_width) {
            LOG_MAIN_ERROR_AT("FFmpegEncoder:CopyPackedFrameToAVFrame: invalid UV planes");
            return false;
        }
        const size_t u_last = u_offset +
            static_cast<size_t>(u_stride) * static_cast<size_t>(uv_height - 1) +
            static_cast<size_t>(uv_width);
        const size_t v_last = v_offset +
            static_cast<size_t>(v_stride) * static_cast<size_t>(uv_height - 1) +
            static_cast<size_t>(uv_width);
        if (u_last > buffer_size || v_last > buffer_size) {
            LOG_MAIN_ERROR_AT("FFmpegEncoder:CopyPackedFrameToAVFrame: UV exceeds buffer");
            return false;
        }

        for (int row = 0; row < uv_height; ++row) {
            const uint8_t* u = base + u_offset + static_cast<size_t>(row) * u_stride;
            const uint8_t* v = base + v_offset + static_cast<size_t>(row) * v_stride;
            uint8_t* uv = dst->data[1] + static_cast<size_t>(row) * dst->linesize[1];
            for (int col = 0; col < uv_width; ++col) {
                uv[col * 2] = u[col];
                uv[col * 2 + 1] = v[col];
            }
        }
        return true;
    };

    switch (fmt) {
        case PixelFormat::kI420: {
            const int y_stride = source_stride(0, width);
            const int uv_width = (width + 1) / 2;
            const int uv_height = (height + 1) / 2;
            const int u_stride = source_stride(1, uv_width);
            const int v_stride = source_stride(2, uv_width);
            const size_t y_size = static_cast<size_t>(y_stride) * height;
            const size_t u_size = static_cast<size_t>(u_stride) * uv_height;
            const size_t y_offset = source_offset(0, 0);
            const size_t u_offset = source_offset(1, y_offset + y_size);
            const size_t v_offset = source_offset(2, u_offset + u_size);
            if (dst->format == AV_PIX_FMT_NV12) {
                // I420 拷贝到 NV12: Y 平面逐行复制，U/V 交错合并
                return copy_plane(0, y_offset, y_stride, width, height) &&
                       interleave_i420_to_nv12(u_offset, u_stride, v_offset, v_stride,
                                               uv_width, uv_height);
            }
            return copy_plane(0, y_offset, y_stride, width, height) &&
                   copy_plane(1, u_offset, u_stride, uv_width, uv_height) &&
                   copy_plane(2, v_offset, v_stride, uv_width, uv_height);
        }
        case PixelFormat::kNV12:
        case PixelFormat::kNV21: {
            const int y_stride = source_stride(0, width);
            const int uv_stride = source_stride(1, width);
            const int uv_height = (height + 1) / 2;
            const size_t y_size = static_cast<size_t>(y_stride) * height;
            const size_t y_offset = source_offset(0, 0);
            const size_t uv_offset = source_offset(1, y_offset + y_size);
            return copy_plane(0, y_offset, y_stride, width, height) &&
                   copy_plane(1, uv_offset, uv_stride, width, uv_height);
        }
        case PixelFormat::kBGR24:
        case PixelFormat::kRGB24: {
            const int row_bytes = width * 3;
            const int stride = source_stride(0, row_bytes);
            const size_t offset = source_offset(0, 0);
            return copy_plane(0, offset, stride, row_bytes, height);
        }
        case PixelFormat::kGRAY8: {
            const int stride = source_stride(0, width);
            const size_t offset = source_offset(0, 0);
            return copy_plane(0, offset, stride, width, height);
        }
        default:
            LOG_MAIN_ERROR_AT("FFmpegEncoder:CopyPackedFrameToAVFrame: unsupported pixel format {}",
                              static_cast<int>(fmt));
            return false;
    }
}

// 解析帧 PTS: 如果帧的 pts 非零则使用之，否则自动分配递增 PTS
int64_t FFmpegEncoder::ResolveFramePts(const MediaFrame& frame) {
    if (frame.pts != 0 || next_pts_ == 0) {
        const int64_t pts = frame.pts;
        next_pts_ = std::max(next_pts_, pts + 1);
        return pts;
    }
    return next_pts_++;
}

// CodecType -> AVCodecID 映射
AVCodecID FFmpegEncoder::MapCodecType(CodecType type) {
    switch (type) {
        case CodecType::H264: return AV_CODEC_ID_H264;
        case CodecType::H265: return AV_CODEC_ID_HEVC;
        case CodecType::AAC:  return AV_CODEC_ID_AAC;
        case CodecType::OPUS: return AV_CODEC_ID_OPUS;
        default:              return AV_CODEC_ID_NONE;
    }
}

// PixelFormat -> AVPixelFormat 映射
AVPixelFormat FFmpegEncoder::MapPixelFormat(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::kNV12:    return AV_PIX_FMT_NV12;
        case PixelFormat::kNV21:    return AV_PIX_FMT_NV21;
        case PixelFormat::kI420:    return AV_PIX_FMT_YUV420P;
        case PixelFormat::kBGR24:   return AV_PIX_FMT_BGR24;
        case PixelFormat::kRGB24:   return AV_PIX_FMT_RGB24;
        case PixelFormat::kGRAY8:   return AV_PIX_FMT_GRAY8;
        default:                    return AV_PIX_FMT_NONE;
    }
}

// AVCodecID -> CodecType 反向映射
CodecType FFmpegEncoder::MapAVCodecID(AVCodecID id) {
    switch (id) {
        case AV_CODEC_ID_H264: return CodecType::H264;
        case AV_CODEC_ID_HEVC: return CodecType::H265;
        case AV_CODEC_ID_AAC:  return CodecType::AAC;
        case AV_CODEC_ID_OPUS: return CodecType::OPUS;
        default:               return CodecType::UNKNOWN;
    }
}

// AVPixelFormat -> PixelFormat 反向映射
PixelFormat FFmpegEncoder::MapAVPixelFormat(AVPixelFormat fmt) {
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
