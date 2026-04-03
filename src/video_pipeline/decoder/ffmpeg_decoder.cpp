#include "video_pipeline/decoder/ffmpeg_decoder.h"
#include "log/logmanager.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

FFmpegDecoder::FFmpegDecoder() {
    // 分配 FFmpeg 结构
    frame_ = av_frame_alloc();
    pkt_ = av_packet_alloc();
    
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

cv::Mat FFmpegDecoder::convertToMat(AVFrame* frame) {
    if (!frame) {
        return cv::Mat();
    }
    
    // 1. 创建图像转换上下文（YUV -> BGR）
    struct SwsContext* sws_ctx = sws_getContext(
        frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
        frame->width, frame->height, AV_PIX_FMT_BGR24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    if (!sws_ctx) {
        LOG_MAIN_ERROR_AT("Failed to create SwsContext");
        return cv::Mat();
    }
    
    // 2. 分配输出缓冲区
    uint8_t* out_data[1] = {nullptr};
    int out_linesize[1] = {0};
    
    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, 
                                             frame->width, frame->height, 1);
    out_data[0] = static_cast<uint8_t*>(av_malloc(num_bytes));
    
    if (!out_data[0]) {
        LOG_MAIN_ERROR_AT("Failed to allocate output buffer");
        sws_freeContext(sws_ctx);
        return cv::Mat();
    }
    
    av_image_fill_arrays(out_data, out_linesize, out_data[0],
                        AV_PIX_FMT_BGR24, frame->width, frame->height, 1);
    
    // 3. 转换图像格式
    sws_scale(sws_ctx, frame->data, frame->linesize, 0,
              frame->height, out_data, out_linesize);
    
    // 4. 创建 OpenCV Mat（共享内存，避免拷贝）
    cv::Mat mat(frame->height, frame->width, CV_8UC3, out_data[0], out_linesize[0]);
    
    // 注意：这里 Mat 使用的是 FFmpeg 分配的内存
    // 需要在 Mat 销毁时释放内存
    // 为简单起见，我们创建一个深拷贝
    cv::Mat mat_copy = mat.clone();
    
    // 5. 清理
    av_free(out_data[0]);
    sws_freeContext(sws_ctx);
    
    return mat_copy;
}

void FFmpegDecoder::processDecodedFrame(AVFrame* av_frame, int64_t pts, FrameCallback cb) {
    if (!av_frame || !cb) {
        return;
    }
    
    // 转换为 OpenCV Mat
    cv::Mat mat = convertToMat(av_frame);
    
    if (mat.empty()) {
        LOG_MAIN_WARN_AT("Converted frame is empty");
        return;
    }
    
    frames_decoded_++;
    
    // 调用回调函数
    cb(std::move(mat), pts);
    
    // 每 100 帧打印一次统计
    if (frames_decoded_.load() % 100 == 0) {
        LOG_MAIN_INFO_AT("Decoded {} frames ({}x{})",
                        frames_decoded_.load(),
                        mat.cols, mat.rows);
    }
}
