#pragma once

#include "video_pipeline/puller/zlm_puller.h"
#include "video_pipeline/decoder/ffmpeg_decoder.h"
#include "video_pipeline/processor/opencv_processor.h"
#include "video_pipeline/frame_queue.h"
#include "video_pipeline/pipeline_config.h"
#include <boost/asio.hpp>
#include <thread>
#include <atomic>

/// @brief 视频处理流水线
/// 将拉流、解码、处理三个环节串联起来
class VideoPipeline {
public:
    /// @brief 构造函数
    /// @param io_ctx io_context
    /// @param config 流水线配置
    explicit VideoPipeline(boost::asio::io_context& io_ctx, const PipelineConfig& config);
    
    /// @brief 析构函数
    ~VideoPipeline();
    
    /// @brief 启动流水线
    /// @return true 成功，false 失败
    bool start();
    
    /// @brief 停止流水线
    void stop();
    
    /// @brief 是否正在运行
    bool isRunning() const { return running_; }
    
    /// @brief 获取通道 ID
    int getChannelId() const { return config_.channel_id; }
    
    /// @brief 获取统计信息
    uint64_t getFramesReceived() const { return frames_received_.load(); }
    uint64_t getFramesDecoded() const { return frames_decoded_.load(); }
    uint64_t getFramesProcessed() const { return frames_processed_.load(); }
    
    /// @brief 设置帧输出回调（算法模块使用）
    using FrameOutputCallback = std::function<void(int channel_id, cv::Mat&& frame, int64_t pts)>;
    void setFrameOutputCallback(FrameOutputCallback cb) { output_callback_ = std::move(cb); }
    
private:
    // ==================== 内部回调处理 ====================
    /// @brief 序列头回调：接收 SPS/PPS 数据
    void onSequenceHeaderReceived(int codec_id, const uint8_t* data, int size);
    
    /// @brief 拉流器回调：接收 NALU 数据
    void onNaluReceived(const uint8_t* data, int size, int64_t pts);
    
    /// @brief 解码器回调：接收解码后的帧
    void onFrameDecoded(cv::Mat&& frame, int64_t pts);
    
    /// @brief 处理器回调：接收处理后的帧
    void onFrameProcessed(cv::Mat&& frame, int64_t pts);
    
    // ==================== 成员变量 ====================
    /// @brief 配置
    PipelineConfig config_;
    
    /// @brief io_context
    boost::asio::io_context& io_ctx_;
    
    /// @brief 拉流器
    std::unique_ptr<ZLMPuller> puller_;
    
    /// @brief 解码器
    std::unique_ptr<FFmpegDecoder> decoder_;
    
    /// @brief 处理器
    std::unique_ptr<OpenCVProcessor> processor_;
    
    /// @brief 原始数据队列（Puller → Decoder）
    std::shared_ptr<RawPacketQueue> raw_queue_;
    
    /// @brief 解码帧队列（Decoder → Processor）
    std::shared_ptr<FrameDataQueue> decoded_queue_;
    
    /// @brief 处理帧队列（Processor → Algorithm）
    std::shared_ptr<FrameDataQueue> processed_queue_;
    
    /// @brief SPS/PPS 数据（H.264，用于初始化解码器和重连恢复）
    std::vector<uint8_t> sps_pps_data_;
    
    /// @brief SPS/PPS 数据（H.265，用于初始化解码器和重连恢复）
    std::vector<uint8_t> sps_pps_h265_data_;
    
    /// @brief 运行状态
    std::atomic<bool> running_{false};
    
    /// @brief 解码器已初始化
    std::atomic<bool> decoder_initialized_{false};
    
    /// @brief 统计信息
    std::atomic<uint64_t> frames_received_{0};
    std::atomic<uint64_t> frames_decoded_{0};
    std::atomic<uint64_t> frames_processed_{0};
    
    /// @brief 输出回调
    FrameOutputCallback output_callback_;
    
    /// @brief 工作线程
    std::thread decoder_thread_;
    std::thread processor_thread_;
};