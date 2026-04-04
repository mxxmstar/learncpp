#include "video_pipeline/video_pipeline.h"
#include "log/logmanager.h"

VideoPipeline::VideoPipeline(boost::asio::io_context& io_ctx, const PipelineConfig& config)
    : config_(config)
    , io_ctx_(io_ctx)
{
    // 1. 创建拉流器
    puller_ = std::make_unique<ZLMPuller>(io_ctx_);
    puller_->setReconnectParams(config_.reconnect_delay, config_.max_reconnect_attempts);
    
    // 2. 创建解码器
    decoder_ = std::make_unique<FFmpegDecoder>();
    decoder_->setThreadCount(config_.decoder_threads);
    
    // 3. 创建处理器
    processor_ = std::make_unique<OpenCVProcessor>(config_.filters);
    
    if (config_.enable_preprocess && config_.target_width > 0 && config_.target_height > 0) {
        processor_->setTargetSize(config_.target_width, config_.target_height);
    }
    
    // 4. 创建队列
    raw_queue_ = std::make_shared<RawPacketQueue>(config_.raw_queue_size);
    decoded_queue_ = std::make_shared<FrameDataQueue>(config_.decoded_queue_size);
    processed_queue_ = std::make_shared<FrameDataQueue>(config_.processed_queue_size);
    
    LOG_MAIN_INFO_AT("VideoPipeline created: channel={}, url={}", 
                    config_.channel_id, config_.stream_url);
}

VideoPipeline::~VideoPipeline() {
    stop();
    LOG_MAIN_INFO_AT("VideoPipeline destroyed");
}

bool VideoPipeline::start() {
    if (running_) {
        LOG_MAIN_WARN_AT("Pipeline already running");
        return false;
    }
    
    try {
        // 1. 启动拉流器（使用新的双回调接口）
        bool success = puller_->start(config_.stream_url,
            [this](int codec_id, const uint8_t* data, int size) {
                onSequenceHeaderReceived(codec_id, data, size);
            },
            [this](const uint8_t* data, int size, int64_t pts) {
                onNaluReceived(data, size, pts);
            });
        
        if (!success) {
            LOG_MAIN_ERROR_AT("Failed to start puller");
            return false;
        }
        
        // 2. 启动解码线程
        decoder_thread_ = std::thread([this]() {
            while (running_) {
                // 从队列中取出 NALU 数据
                auto packet_opt = raw_queue_->pop(std::chrono::milliseconds(100));
                if (!packet_opt) {
                    continue;
                }
                
                auto& packet = *packet_opt;
                
                // 如果解码器未初始化，跳过（等待序列头回调完成初始化）
                if (!decoder_initialized_) {
                    LOG_MAIN_DEBUG_AT("Decoder not initialized yet, skipping frame");
                    continue;
                }
                
                // 解码 NALU
                decoder_->decode(packet.data.data(), static_cast<int>(packet.data.size()), packet.pts,
                    [this](cv::Mat&& frame, int64_t pts) {
                        onFrameDecoded(std::move(frame), pts);
                    });
            }
        });
        
        // 3. 启动处理线程
        processor_thread_ = std::thread([this]() {
            while (running_) {
                // 从队列中取出解码后的帧
                auto frame_opt = decoded_queue_->pop(std::chrono::milliseconds(100));
                if (!frame_opt) {
                    continue;
                }
                
                auto& frame_data = *frame_opt;
                
                // 处理帧
                cv::Mat processed_frame = processor_->process(std::move(frame_data.frame));
                
                // 调用处理完成回调
                onFrameProcessed(std::move(processed_frame), frame_data.pts);
            }
        });
        
        running_ = true;
        LOG_MAIN_INFO_AT("VideoPipeline started: channel={}", config_.channel_id);
        return true;
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to start pipeline: {}", e.what());
        stop();
        return false;
    }
}

void VideoPipeline::stop() {
    if (!running_) {
        LOG_MAIN_WARN_AT("Pipeline already stopped");
        return;
    }
    
    running_ = false;
    
    // 1. 停止拉流器
    puller_->stop();
    
    // 2. 停止解码线程
    if (decoder_thread_.joinable()) {
        decoder_thread_.join();
    }
    
    // 3. 停止处理线程
    if (processor_thread_.joinable()) {
        processor_thread_.join();
    }
    
    // 4. 关闭解码器
    decoder_->close();
    
    LOG_MAIN_INFO_AT("VideoPipeline stopped: received={}, decoded={}, processed={}",
                    frames_received_.load(),
                    frames_decoded_.load(),
                    frames_processed_.load());
    
}

/// @brief 序列头回调：接收 SPS/PPS 数据
void VideoPipeline::onSequenceHeaderReceived(int codec_id, const uint8_t* data, int size) {
    LOG_MAIN_INFO_AT("Received sequence header: codec={}, size={}", codec_id, size);
    
    // 根据编解码器类型分别处理
    if (codec_id == 7) {  // H.264
        // 保存 H.264 SPS/PPS 数据
        sps_pps_data_.assign(data, data + size);
        
        // 验证 AVCC 格式并初始化 H.264 解码器
        if (!decoder_initialized_ && sps_pps_data_.size() > 10) {
            if (sps_pps_data_[0] != 1) {
                LOG_MAIN_WARN_AT("Invalid AVCC version for H.264: {}", sps_pps_data_[0]);
            }
            
            if (decoder_->open(sps_pps_data_.data(), 
                              sps_pps_data_.size(), 
                              27)) {  // 27 = AV_CODEC_ID_H264
                decoder_initialized_ = true;
                LOG_MAIN_INFO_AT("H.264 Decoder initialized with {} bytes of AVCC data", 
                               sps_pps_data_.size());
            } else {
                LOG_MAIN_ERROR_AT("Failed to initialize H.264 decoder");
            }
        }
    } 
    else if (codec_id == 12) {  // H.265
        // 保存 H.265 VPS/SPS/PPS 数据
        sps_pps_h265_data_.assign(data, data + size);
        
        // 验证 AVCC 格式并初始化 H.265 解码器
        if (!decoder_initialized_ && sps_pps_h265_data_.size() > 10) {
            if (sps_pps_h265_data_[0] != 1) {
                LOG_MAIN_WARN_AT("Invalid HVCC version for H.265: {}", sps_pps_h265_data_[0]);
            }
            
            if (decoder_->open(sps_pps_h265_data_.data(), 
                              sps_pps_h265_data_.size(), 
                              173)) {  // 173 = AV_CODEC_ID_H265
                decoder_initialized_ = true;
                LOG_MAIN_INFO_AT("H.265 Decoder initialized with {} bytes of HVCC data", 
                               sps_pps_h265_data_.size());
            } else {
                LOG_MAIN_ERROR_AT("Failed to initialize H.265 decoder");
            }
        }
    }
    else {
        LOG_MAIN_WARN_AT("Unsupported codec ID: {}", codec_id);
    }
}

/// @brief 拉流器回调：接收 NALU 数据
void VideoPipeline::onNaluReceived(const uint8_t* data, int size, int64_t pts) {
    frames_received_++;
    
    // 将 NALU 数据推入队列
    RawPacketData packet(config_.channel_id, pts, data, size);
    if (!raw_queue_->push(std::move(packet))) {
        // 队列已满，丢弃
        // TODO: 后续优化！！！！！！！！！！
        static int dropped = 0;
        if (++dropped % 100 == 0) {
            LOG_MAIN_WARN_AT("Raw queue full, dropped {} frames", dropped);
        }
    }
}

/// @brief 解码器回调：接收解码后的帧
void VideoPipeline::onFrameDecoded(cv::Mat&& frame, int64_t pts) {
    frames_decoded_++;
    
    // 将解码后的帧推入队列
    FrameData frame_data(config_.channel_id, pts, std::move(frame));
    if (!decoded_queue_->push(std::move(frame_data))) {
        // 队列已满，丢弃
        static int dropped = 0;
        if (++dropped % 100 == 0) {
            LOG_MAIN_WARN_AT("Decoded queue full, dropped {} frames", dropped);
        }
    }
}

/// @brief 处理器回调：接收处理后的帧
void VideoPipeline::onFrameProcessed(cv::Mat&& frame, int64_t pts) {
    frames_processed_++;
    
    // 调用输出回调（传递给算法模块）
    if (output_callback_) {
        output_callback_(config_.channel_id, std::move(frame), pts);
    }
    
    // 将处理后的帧推入队列（如果需要缓冲）
    FrameData frame_data(config_.channel_id, pts, std::move(frame));
    if (!processed_queue_->push(std::move(frame_data))) {
        // 队列已满，丢弃
        static int dropped = 0;
        if (++dropped % 100 == 0) {
            LOG_MAIN_WARN_AT("Processed queue full, dropped {} frames", dropped);
        }
    }
    
    // 打印统计信息（每 100 帧）
    if (frames_processed_.load() % 100 == 0) {
        LOG_MAIN_INFO_AT("Channel {}: received={}, decoded={}, processed={}",
                        config_.channel_id,
                        frames_received_.load(),
                        frames_decoded_.load(),
                        frames_processed_.load());
    }
}
