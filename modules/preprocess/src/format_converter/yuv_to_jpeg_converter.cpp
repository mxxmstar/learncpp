#include "preprocess/format_converter/yuv_to_jpeg_converter.h"
#include "common/log/logmanager.h"
#include <cstring>

YuvToJpegConverter::YuvToJpegConverter(int quality)
    : quality_(quality)
    , encoder_ctx_(nullptr)
    , initialized_(false) {
    
    // 限制质量范围
    if (quality_ < 1) quality_ = 1;
    if (quality_ > 100) quality_ = 100;
    
    // 初始化编码器
    if (!InitEncoder()) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to initialize encoder");
    }
}

YuvToJpegConverter::~YuvToJpegConverter() {
    CleanupEncoder();
}

bool YuvToJpegConverter::InitEncoder() {
    // 查找 MJPEG 编码器
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!codec) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] MJPEG encoder not found");
        return false;
    }
    
    // 创建编码器上下文
    encoder_ctx_ = avcodec_alloc_context3(codec);
    if (!encoder_ctx_) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to allocate encoder context");
        return false;
    }
    
    // 设置编码器参数
    encoder_ctx_->pix_fmt = AV_PIX_FMT_YUVJ420P;  // JPEG 使用的像素格式
    encoder_ctx_->qmin = quality_;  // 最小量化参数
    encoder_ctx_->qmax = quality_;  // 最大量化参数
    
    // 打开编码器
    int ret = avcodec_open2(encoder_ctx_, codec, nullptr);
    if (ret < 0) {
        char err_buf[256];
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to open encoder: {}", err_buf);
        avcodec_free_context(&encoder_ctx_);
        return false;
    }
    
    initialized_ = true;
    return true;
}

void YuvToJpegConverter::CleanupEncoder() {
    if (encoder_ctx_) {
        avcodec_free_context(&encoder_ctx_);
        encoder_ctx_ = nullptr;
    }
    initialized_ = false;
}

bool YuvToJpegConverter::ConvertYuv420p(const uint8_t* y_data,
                                       const uint8_t* u_data,
                                       const uint8_t* v_data,
                                       int width,
                                       int height,
                                       std::vector<uint8_t>& jpeg_output) {
    if (!initialized_ || !encoder_ctx_) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Encoder not initialized");
        return false;
    }
    
    // 验证输入参数
    if (!y_data || !u_data || !v_data || width <= 0 || height <= 0) {
        LOG_MAIN_WARN_AT("[YuvToJpegConverter] Invalid input parameters");
        return false;
    }
    
    // 更新编码器尺寸（如果改变）
    if (encoder_ctx_->width != width || encoder_ctx_->height != height) {
        CleanupEncoder();
        if (!InitEncoder()) {
            return false;
        }
    }
    encoder_ctx_->width = width;
    encoder_ctx_->height = height;
    
    // 创建输入帧
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to allocate frame");
        return false;
    }
    
    frame->format = AV_PIX_FMT_YUVJ420P;
    frame->width = width;
    frame->height = height;
    
    // 分配帧缓冲区
    int ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        char err_buf[256];
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to allocate frame buffer: {}", err_buf);
        av_frame_free(&frame);
        return false;
    }
    
    // 复制 YUV 数据到帧
    // Y 平面
    for (int i = 0; i < height; i++) {
        memcpy(frame->data[0] + i * frame->linesize[0], 
               y_data + i * width, 
               width);
    }
    
    // U 平面
    int uv_width = width / 2;
    int uv_height = height / 2;
    for (int i = 0; i < uv_height; i++) {
        memcpy(frame->data[1] + i * frame->linesize[1], 
               u_data + i * uv_width, 
               uv_width);
    }
    
    // V 平面
    for (int i = 0; i < uv_height; i++) {
        memcpy(frame->data[2] + i * frame->linesize[2], 
               v_data + i * uv_width, 
               uv_width);
    }
    
    // 编码为 JPEG
    bool success = EncodeFrame(frame, jpeg_output);
    
    // 清理
    av_frame_free(&frame);
    
    return success;
}

size_t YuvToJpegConverter::ConvertYuv420pZeroCopy(const uint8_t* y_data,
                                                 const uint8_t* u_data,
                                                 const uint8_t* v_data,
                                                 int width,
                                                 int height,
                                                 uint8_t* output_buffer,
                                                 size_t buffer_capacity) {
    if (!initialized_ || !encoder_ctx_) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Encoder not initialized");
        return 0;
    }
    
    // 验证输入参数
    if (!y_data || !u_data || !v_data || !output_buffer || width <= 0 || height <= 0 || buffer_capacity == 0) {
        LOG_MAIN_WARN_AT("[YuvToJpegConverter] Invalid input parameters for zero-copy conversion");
        return 0;
    }
    
    // 更新编码器尺寸（如果改变）
    if (encoder_ctx_->width != width || encoder_ctx_->height != height) {
        CleanupEncoder();
        if (!InitEncoder()) {
            return 0;
        }
    }
    encoder_ctx_->width = width;
    encoder_ctx_->height = height;
    
    // 创建输入帧
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to allocate frame");
        return 0;
    }
    
    frame->format = AV_PIX_FMT_YUVJ420P;
    frame->width = width;
    frame->height = height;
    
    // 分配帧缓冲区
    int ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        char err_buf[256];
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to allocate frame buffer: {}", err_buf);
        av_frame_free(&frame);
        return 0;
    }
    
    // 复制 YUV 数据到帧
    // Y 平面
    for (int i = 0; i < height; i++) {
        memcpy(frame->data[0] + i * frame->linesize[0], 
               y_data + i * width, 
               width);
    }
    
    // U 平面
    int uv_width = width / 2;
    int uv_height = height / 2;
    for (int i = 0; i < uv_height; i++) {
        memcpy(frame->data[1] + i * frame->linesize[1], 
               u_data + i * uv_width, 
               uv_width);
    }
    
    // V 平面
    for (int i = 0; i < uv_height; i++) {
        memcpy(frame->data[2] + i * frame->linesize[2], 
               v_data + i * uv_width, 
               uv_width);
    }
    
    // 发送帧到编码器
    ret = avcodec_send_frame(encoder_ctx_, frame);
    if (ret < 0) {
        char err_buf[256];
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Error sending frame: {}", err_buf);
        av_frame_free(&frame);
        return 0;
    }
    
    // 接收编码后的数据包
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to allocate packet");
        av_frame_free(&frame);
        return 0;
    }
    
    ret = avcodec_receive_packet(encoder_ctx_, pkt);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            LOG_MAIN_WARN_AT("[YuvToJpegConverter] Need more data or encoder finished");
        } else {
            char err_buf[256];
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Error receiving packet: {}", err_buf);
        }
        av_packet_free(&pkt);
        av_frame_free(&frame);
        return 0;
    }
    
    // ✅ 零拷贝：检查缓冲区是否足够
    if (static_cast<size_t>(pkt->size) > buffer_capacity) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Output buffer too small: need {}, have {}", 
                         pkt->size, buffer_capacity);
        av_packet_free(&pkt);
        av_frame_free(&frame);
        return 0;
    }
    
    // ✅ 零拷贝：直接复制到调用者提供的缓冲区
    memcpy(output_buffer, pkt->data, pkt->size);
    size_t jpeg_size = static_cast<size_t>(pkt->size);
    
    // 清理
    av_packet_free(&pkt);
    av_frame_free(&frame);
    
    return jpeg_size;
}

bool YuvToJpegConverter::ConvertNv12(const uint8_t* y_data,
                                    const uint8_t* uv_data,
                                    int width,
                                    int height,
                                    std::vector<uint8_t>& jpeg_output) {
    if (!initialized_ || !encoder_ctx_) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Encoder not initialized");
        return false;
    }
    
    // 验证输入参数
    if (!y_data || !uv_data || width <= 0 || height <= 0) {
        LOG_MAIN_WARN_AT("[YuvToJpegConverter] Invalid input parameters");
        return false;
    }
    
    // 使用 sws_scale 直接将 NV12 转换为 YUVJ420P
    SwsContext* sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_NV12,      // 源格式
        width, height, AV_PIX_FMT_YUVJ420P,  // 目标格式
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!sws_ctx) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to create SwsContext for NV12");
        return false;
    }
    
    // 准备源数据指针
    const uint8_t* src_data[2] = { y_data, uv_data };
    int src_linesize[2] = { width, width };
    
    // 创建目标帧
    AVFrame* dst_frame = av_frame_alloc();
    dst_frame->format = AV_PIX_FMT_YUVJ420P;
    dst_frame->width = width;
    dst_frame->height = height;
    av_frame_get_buffer(dst_frame, 32);
    
    // 执行转换
    sws_scale(sws_ctx,
              src_data, src_linesize,
              0, height,
              dst_frame->data, dst_frame->linesize);
    
    // 编码为 JPEG
    bool success = EncodeFrame(dst_frame, jpeg_output);
    
    // 清理
    av_frame_free(&dst_frame);
    sws_freeContext(sws_ctx);
    
    return success;
}

size_t YuvToJpegConverter::ConvertNv12ZeroCopy(const uint8_t* y_data,
                                              const uint8_t* uv_data,
                                              int width,
                                              int height,
                                              uint8_t* output_buffer,
                                              size_t buffer_capacity) {
    if (!initialized_ || !encoder_ctx_) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Encoder not initialized");
        return 0;
    }
    
    // 验证输入参数
    if (!y_data || !uv_data || !output_buffer || width <= 0 || height <= 0 || buffer_capacity == 0) {
        LOG_MAIN_WARN_AT("[YuvToJpegConverter] Invalid input parameters for zero-copy conversion");
        return 0;
    }
    
    // 使用 sws_scale 直接将 NV12 转换为 YUVJ420P
    SwsContext* sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_NV12,      // 源格式
        width, height, AV_PIX_FMT_YUVJ420P,  // 目标格式
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!sws_ctx) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to create SwsContext for NV12");
        return 0;
    }
    
    // 准备源数据指针
    const uint8_t* src_data[2] = { y_data, uv_data };
    int src_linesize[2] = { width, width };
    
    // 创建目标帧
    AVFrame* dst_frame = av_frame_alloc();
    dst_frame->format = AV_PIX_FMT_YUVJ420P;
    dst_frame->width = width;
    dst_frame->height = height;
    av_frame_get_buffer(dst_frame, 32);
    
    // 执行转换
    sws_scale(sws_ctx,
              src_data, src_linesize,
              0, height,
              dst_frame->data, dst_frame->linesize);
    
    // 发送帧到编码器
    int ret = avcodec_send_frame(encoder_ctx_, dst_frame);
    if (ret < 0) {
        char err_buf[256];
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Error sending frame: {}", err_buf);
        av_frame_free(&dst_frame);
        sws_freeContext(sws_ctx);
        return 0;
    }
    
    // 接收编码后的数据包
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to allocate packet");
        av_frame_free(&dst_frame);
        sws_freeContext(sws_ctx);
        return 0;
    }
    
    ret = avcodec_receive_packet(encoder_ctx_, pkt);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            LOG_MAIN_WARN_AT("[YuvToJpegConverter] Need more data or encoder finished");
        } else {
            char err_buf[256];
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Error receiving packet: {}", err_buf);
        }
        av_packet_free(&pkt);
        av_frame_free(&dst_frame);
        sws_freeContext(sws_ctx);
        return 0;
    }
    
    // ✅ 零拷贝：检查缓冲区是否足够
    if (static_cast<size_t>(pkt->size) > buffer_capacity) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Output buffer too small: need {}, have {}", 
                         pkt->size, buffer_capacity);
        av_packet_free(&pkt);
        av_frame_free(&dst_frame);
        sws_freeContext(sws_ctx);
        return 0;
    }
    
    // ✅ 零拷贝：直接复制到调用者提供的缓冲区
    memcpy(output_buffer, pkt->data, pkt->size);
    size_t jpeg_size = static_cast<size_t>(pkt->size);
    
    // 清理
    av_packet_free(&pkt);
    av_frame_free(&dst_frame);
    sws_freeContext(sws_ctx);
    
    return jpeg_size;
}

bool YuvToJpegConverter::ConvertNv21(const uint8_t* y_data,
                                    const uint8_t* vu_data,
                                    int width,
                                    int height,
                                    std::vector<uint8_t>& jpeg_output) {
    if (!initialized_ || !encoder_ctx_) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Encoder not initialized");
        return false;
    }
    
    // 验证输入参数
    if (!y_data || !vu_data || width <= 0 || height <= 0) {
        LOG_MAIN_WARN_AT("[YuvToJpegConverter] Invalid input parameters");
        return false;
    }
    
    // 使用 sws_scale 直接将 NV21 转换为 YUVJ420P
    SwsContext* sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_NV21,      // 源格式
        width, height, AV_PIX_FMT_YUVJ420P,  // 目标格式
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!sws_ctx) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to create SwsContext for NV21");
        return false;
    }
    
    // 准备源数据指针
    const uint8_t* src_data[2] = { y_data, vu_data };
    int src_linesize[2] = { width, width };
    
    // 创建目标帧
    AVFrame* dst_frame = av_frame_alloc();
    dst_frame->format = AV_PIX_FMT_YUVJ420P;
    dst_frame->width = width;
    dst_frame->height = height;
    av_frame_get_buffer(dst_frame, 32);
    
    // 执行转换
    sws_scale(sws_ctx,
              src_data, src_linesize,
              0, height,
              dst_frame->data, dst_frame->linesize);
    
    // 编码为 JPEG
    bool success = EncodeFrame(dst_frame, jpeg_output);
    
    // 清理
    av_frame_free(&dst_frame);
    sws_freeContext(sws_ctx);
    
    return success;
}

size_t YuvToJpegConverter::ConvertNv21ZeroCopy(const uint8_t* y_data,
                                              const uint8_t* vu_data,
                                              int width,
                                              int height,
                                              uint8_t* output_buffer,
                                              size_t buffer_capacity) {
    if (!initialized_ || !encoder_ctx_) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Encoder not initialized");
        return 0;
    }
    
    // 验证输入参数
    if (!y_data || !vu_data || !output_buffer || width <= 0 || height <= 0 || buffer_capacity == 0) {
        LOG_MAIN_WARN_AT("[YuvToJpegConverter] Invalid input parameters for zero-copy conversion");
        return 0;
    }
    
    // 使用 sws_scale 直接将 NV21 转换为 YUVJ420P
    SwsContext* sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_NV21,      // 源格式
        width, height, AV_PIX_FMT_YUVJ420P,  // 目标格式
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!sws_ctx) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to create SwsContext for NV21");
        return 0;
    }
    
    // 准备源数据指针
    const uint8_t* src_data[2] = { y_data, vu_data };
    int src_linesize[2] = { width, width };
    
    // 创建目标帧
    AVFrame* dst_frame = av_frame_alloc();
    dst_frame->format = AV_PIX_FMT_YUVJ420P;
    dst_frame->width = width;
    dst_frame->height = height;
    av_frame_get_buffer(dst_frame, 32);
    
    // 执行转换
    sws_scale(sws_ctx,
              src_data, src_linesize,
              0, height,
              dst_frame->data, dst_frame->linesize);
    
    // 发送帧到编码器
    int ret = avcodec_send_frame(encoder_ctx_, dst_frame);
    if (ret < 0) {
        char err_buf[256];
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Error sending frame: {}", err_buf);
        av_frame_free(&dst_frame);
        sws_freeContext(sws_ctx);
        return 0;
    }
    
    // 接收编码后的数据包
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to allocate packet");
        av_frame_free(&dst_frame);
        sws_freeContext(sws_ctx);
        return 0;
    }
    
    ret = avcodec_receive_packet(encoder_ctx_, pkt);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            LOG_MAIN_WARN_AT("[YuvToJpegConverter] Need more data or encoder finished");
        } else {
            char err_buf[256];
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Error receiving packet: {}", err_buf);
        }
        av_packet_free(&pkt);
        av_frame_free(&dst_frame);
        sws_freeContext(sws_ctx);
        return 0;
    }
    
    // ✅ 零拷贝：检查缓冲区是否足够
    if (static_cast<size_t>(pkt->size) > buffer_capacity) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Output buffer too small: need {}, have {}", 
                         pkt->size, buffer_capacity);
        av_packet_free(&pkt);
        av_frame_free(&dst_frame);
        sws_freeContext(sws_ctx);
        return 0;
    }
    
    // ✅ 零拷贝：直接复制到调用者提供的缓冲区
    memcpy(output_buffer, pkt->data, pkt->size);
    size_t jpeg_size = static_cast<size_t>(pkt->size);
    
    // 清理
    av_packet_free(&pkt);
    av_frame_free(&dst_frame);
    sws_freeContext(sws_ctx);
    
    return jpeg_size;
}

void YuvToJpegConverter::SetQuality(int quality) {
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;
    
    if (quality_ != quality) {
        quality_ = quality;
        
        // 重新初始化编码器以应用新质量
        if (initialized_) {
            CleanupEncoder();
            InitEncoder();
        }
    }
}

bool YuvToJpegConverter::EncodeFrame(AVFrame* frame, std::vector<uint8_t>& jpeg_output) {
    // 发送帧到编码器
    int ret = avcodec_send_frame(encoder_ctx_, frame);
    if (ret < 0) {
        char err_buf[256];
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Error sending frame: {}", err_buf);
        return false;
    }
    
    // 接收编码后的数据包
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Failed to allocate packet");
        return false;
    }
    
    ret = avcodec_receive_packet(encoder_ctx_, pkt);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            LOG_MAIN_WARN_AT("[YuvToJpegConverter] Need more data or encoder finished");
        } else {
            char err_buf[256];
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOG_MAIN_ERROR_AT("[YuvToJpegConverter] Error receiving packet: {}", err_buf);
        }
        av_packet_free(&pkt);
        return false;
    }
    
    // 复制 JPEG 数据
    jpeg_output.assign(pkt->data, pkt->data + pkt->size);
    
    // 清理
    av_packet_free(&pkt);
    
    return true;
}
