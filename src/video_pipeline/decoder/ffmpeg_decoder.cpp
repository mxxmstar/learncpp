#include "video_pipeline/decoder/ffmpeg_decoder.h"
#include "log/logmanager.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

FFmpegDecoder::FFmpegDecoder() {
    // 分配 FFmpeg 结构
    frame_ = av_frame_alloc();
    pkt_ = av_packet_alloc();
    thread_count_ = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 2;
    if (!frame_ || !pkt_) {
        LOG_MAIN_CRITICAL_AT("Failed to allocate FFmpeg structures");
        close();
        throw std::runtime_error("Failed to allocate FFmpeg structures");
    }
    
    LOG_MAIN_INFO_AT("FFmpegDecoder created (version: {})", 
                    av_version_info());
}

FFmpegDecoder::~FFmpegDecoder() {
    close();
    
    // 释放 FFmpeg 结构
    if (frame_) {
        av_frame_free(&frame_);
    }
    if (pkt_) {
        av_packet_free(&pkt_);
    }
    
    LOG_MAIN_INFO_AT("FFmpegDecoder destroyed. Decoded {} frames from {} packets",
                    frames_decoded_.load(),
                    packets_decoded_.load());
}

bool FFmpegDecoder::open(const uint8_t* extradata, int extradata_size, int codec_id) {
    if (opened_) {
        LOG_MAIN_WARN_AT("Decoder already opened");
        return true;
    }
    
    try {
        // 1. 查找解码器
        const AVCodec* codec = avcodec_find_decoder(static_cast<AVCodecID>(codec_id));
        if (!codec) {
            LOG_MAIN_ERROR_AT("Codec not found: codec_id={}", codec_id);
            return false;
        }
        
        codec_name_ = codec->name;
        codec_id_ = codec_id;
        
        LOG_MAIN_INFO_AT("Found codec: {}", codec_name_);
        
        // 2. 创建编解码器上下文
        codec_ctx_ = avcodec_alloc_context3(codec);
        if (!codec_ctx_) {
            LOG_MAIN_ERROR_AT("Failed to allocate codec context");
            return false;
        }
        
        // 3. 设置额外数据（SPS/PPS）
        if (extradata && extradata_size > 0) {
            codec_ctx_->extradata = static_cast<uint8_t*>(av_malloc(extradata_size + AV_INPUT_BUFFER_PADDING_SIZE));
            if (!codec_ctx_->extradata) {
                LOG_MAIN_ERROR_AT("Failed to allocate extradata");
                return false;
            }
            
            memcpy(codec_ctx_->extradata, extradata, extradata_size);
            codec_ctx_->extradata_size = extradata_size;
            
            LOG_MAIN_INFO_AT("Set extradata: {} bytes", extradata_size);
        }
        
        // 4. 设置解码线程数
        codec_ctx_->thread_count = thread_count_;
        
        // 5. 优化解码延迟
        codec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
        codec_ctx_->flags2 |= AV_CODEC_FLAG2_FAST;
        
        // 6. 打开解码器
        int ret = avcodec_open2(codec_ctx_, codec, nullptr);
        if (ret < 0) {
            char err_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOG_MAIN_ERROR_AT("Failed to open codec: {}", err_buf);
            return false;
        }
        
        opened_ = true;
        
        LOG_MAIN_INFO_AT("Decoder opened successfully: {} (threads={}, low_delay=true)",
                        codec_name_, thread_count_);
        return true;
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Exception while opening decoder: {}", e.what());
        close();
        return false;
    }
}

void FFmpegDecoder::decode(const uint8_t* packet, int size, int64_t pts, FrameCallback cb) {
    if (!opened_) {
        LOG_MAIN_WARN_AT("Decoder not opened");
        return;
    }
    
    if (!packet || size <= 0) {
        LOG_MAIN_WARN_AT("Invalid packet data");
        return;
    }
    
    packets_decoded_++;
    
    try {
        // 1. 准备 AVPacket
        av_packet_unref(pkt_);
        
        // 分配缓冲区并复制数据
        int ret = av_new_packet(pkt_, size);
        if (ret < 0) {
            LOG_MAIN_ERROR_AT("Failed to allocate packet buffer");
            return;
        }
        
        memcpy(pkt_->data, packet, size);
        pkt_->pts = pts;
        pkt_->dts = pts;
        
        // 2. 发送数据包到解码器
        ret = avcodec_send_packet(codec_ctx_, pkt_);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
                // 需要先从解码器取出帧
                LOG_MAIN_DEBUG_AT("Decoder buffer full, need to drain frames");
            }
            else if (ret == AVERROR_EOF) {
                LOG_MAIN_DEBUG_AT("Decoder reached EOF");
            }
            else {
                char err_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, err_buf, sizeof(err_buf));
                LOG_MAIN_ERROR_AT("Failed to send packet: {}", err_buf);
            }
            return;
        }
        
        // 3. 接收解码后的帧
        while (true) {
            ret = avcodec_receive_frame(codec_ctx_, frame_);
            if (ret == AVERROR(EAGAIN)) {
                // 需要更多数据包
                break;
            }
            else if (ret == AVERROR_EOF) {
                // 解码结束
                LOG_MAIN_INFO_AT("Decoder reached end of stream");
                break;
            }
            else if (ret < 0) {
                char err_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, err_buf, sizeof(err_buf));
                LOG_MAIN_ERROR_AT("Failed to receive frame: {}", err_buf);
                break;
            }
            
            // 4. 处理解码后的帧
            processDecodedFrame(frame_, pts, cb);
            
            // 5. 清理帧数据
            av_frame_unref(frame_);
        }
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Exception while decoding: {}", e.what());
    }
}

void FFmpegDecoder::close() {
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
    }
    
    opened_ = false;
    
    LOG_MAIN_INFO_AT("Decoder closed");
}

VideoFrame FFmpegDecoder::convertToVideoFrame(AVFrame* av_frame) {
    if (!av_frame) {
        return VideoFrame();
    }
    
    VideoFrame frame;
    frame.width = av_frame->width;
    frame.height = av_frame->height;
    frame.format = av_frame->format;
    frame.pts = av_frame->pts;
    
    // 深拷贝数据（每个平面）
    for (int i = 0; i < 4; ++i) {
        if (av_frame->data[i] && av_frame->linesize[i] > 0) {
            // 计算需要复制的字节数
            int plane_height = (i == 0) ? av_frame->height : av_frame->height / 2;
            int bytes_to_copy = av_frame->linesize[i] * plane_height;
            
            // 分配内存并复制
            frame.data[i] = static_cast<uint8_t*>(av_malloc(bytes_to_copy));
            if (frame.data[i]) {
                memcpy(frame.data[i], av_frame->data[i], bytes_to_copy);
                frame.linesize[i] = av_frame->linesize[i];
            }
        }
    }
    
    return frame;
}

void FFmpegDecoder::processDecodedFrame(AVFrame* av_frame, int64_t pts, FrameCallback cb) {
    if (!av_frame || !cb) {
        return;
    }
    
    // 转换为通用帧结构
    VideoFrame frame = convertToVideoFrame(av_frame);
    
    if (frame.width == 0 || frame.height == 0) {
        LOG_MAIN_WARN_AT("Converted frame is empty");
        return;
    }
    
    frames_decoded_++;
    
    // 调用回调函数
    cb(std::move(frame));
    
    // 每 100 帧打印一次统计
    if (frames_decoded_.load() % 100 == 0) {
        LOG_MAIN_INFO_AT("Decoded {} frames ({}x{})",
                        frames_decoded_.load(),
                        frame.width, frame.height);
    }
}
