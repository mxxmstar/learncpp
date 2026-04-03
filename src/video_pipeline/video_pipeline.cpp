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
        // 1. 启动拉流器
        bool success = puller_->start(config_.stream_url,
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
                
                // 检查是否是 SPS/PPS（用于初始化解码器）
                if (!decoder_initialized_ && !packet.data.empty()) {
                    // 简单判断：第一个字节是 0x67 (SPS) 或 0x68 (PPS)
                    uint8_t nalu_type = packet.data[0] & 0x1F;
                    if (nalu_type == 7 || nalu_type == 8) {
                        // 收集 SPS/PPS
                        sps_pps_data_.insert(sps_pps_data_.end(),
                                           packet.data.begin(), packet.data.end());
                        
                        // 当收集到足够的 SPS/PPS 后，初始化解码器
                        if (sps_pps_data_.size() > 20) {  // 至少要有 SPS+PPS
                            // 构造 AVCC 格式的 extradata
                            std::vector<uint8_t> extradata;
                            // TODO: 这里需要构造标准的 AVCC 格式
                            // 为简化实现，直接使用 SPS+PPS
                            
                            if (decoder_->open(sps_pps_data_.data(), 
                                             sps_pps_data_.size(), 
                                             27)) {  // 27 = H.264
                                decoder_initialized_ = true;
                                LOG_MAIN_INFO_AT("Decoder initialized");
                            }
                        }
                    }
                }
                
                // 如果解码器未初始化，跳过
                if (!decoder_initialized_) {
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
                
                // 处理图像
                cv::Mat processed = processor_->process(std::move(frame_data.frame));
                
                if (!processed.empty()) {
                    onFrameProcessed(std::move(processed), frame_data.timestamp_us);
                }
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
        return;
    }
    
    running_ = false;
    
    // 1. 停止拉流器
    puller_->stop();
    
    // 2. 等待解码线程结束
    if (decoder_thread_.joinable()) {
        decoder_thread_.join();
    }
    
    // 3. 等待处理线程结束
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

void VideoPipeline::onNaluReceived(const uint8_t* data, int size, int64_t pts) {
    frames_received_++;
    
    // 将 NALU 数据推入队列
    RawPacketData packet(config_.channel_id, pts, data, size);
    if (!raw_queue_->push(std::move(packet))) {
        // 队列已满，丢弃
        static int dropped = 0;
        if (++dropped % 100 == 0) {
            LOG_MAIN_WARN_AT("Raw queue full, dropped {} frames", dropped);
        }
    }
}

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
