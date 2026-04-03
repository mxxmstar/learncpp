#pragma once

#include "video_pipeline/decoder/i_decoder.h"
#include <opencv2/opencv.hpp>
#include <memory>
#include <atomic>

// FFmpeg 前向声明
struct AVCodecContext;
struct AVFrame;
struct AVPacket;

/// @brief FFmpeg 视频解码器
/// 将 H.264/H.265 NALU 数据解码为 OpenCV Mat
class FFmpegDecoder : public IDecoder {
public:
    /// @brief 构造函数
    FFmpegDecoder();
    
    /// @brief 析构函数
    ~FFmpegDecoder() override;
    
    /// @brief 打开解码器
    /// @param extradata 额外数据（SPS/PPS 等）
    /// @param extradata_size 额外数据大小
    /// @param codec_id 编解码器 ID（AV_CODEC_ID_H264=27, AV_CODEC_ID_HEVC=173）
    /// @return true 成功，false 失败
    bool open(const uint8_t* extradata, int extradata_size, int codec_id) override;
    
    /// @brief 解码数据包
    /// @param packet NALU 数据包
    /// @param size 数据包大小
    /// @param pts 显示时间戳
    /// @param cb 帧回调函数
    void decode(const uint8_t* packet, int size, int64_t pts, FrameCallback cb) override;
    
    /// @brief 关闭解码器
    void close() override;
    
    /// @brief 是否已打开
    bool isOpened() const { return opened_; }
    
    /// @brief 获取编解码器名称
    std::string getCodecName() const { return codec_name_; }
    
    /// @brief 设置解码线程数
    void setThreadCount(int count) { thread_count_ = count; }
    
    /// @brief 获取统计信息
    uint64_t getPacketsDecoded() const { return packets_decoded_.load(); }
    uint64_t getFramesDecoded() const { return frames_decoded_.load(); }
    
private:
    /// @brief 将 AVFrame 转换为 cv::Mat
    /// @param frame FFmpeg 帧
    /// @return OpenCV Mat（BGR 格式）
    cv::Mat convertToMat(AVFrame* frame);
    
    /// @brief 处理解码后的帧
    /// @param av_frame 解码后的帧
    /// @param pts 时间戳
    /// @param cb 回调函数
    void processDecodedFrame(AVFrame* av_frame, int64_t pts, FrameCallback cb);
    
    // FFmpeg 上下文
    AVCodecContext* codec_ctx_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVPacket* pkt_ = nullptr;
    
    // 编解码器信息
    std::string codec_name_;
    int codec_id_ = 0;
    
    // 配置
    int thread_count_ = 2;  // 解码线程数
    
    // 状态
    std::atomic<bool> opened_{false};
    
    // 统计信息
    std::atomic<uint64_t> packets_decoded_{0};
    std::atomic<uint64_t> frames_decoded_{0};
};
