#include "videopipeline/video_pipeline.h"
#include "preprocess/format_converter/opencv_format_converter.h"  // 可选组件
#include "preprocess/format_converter/yuv_to_bgr_converter.h"     // YUV 到 BGR 转换
#include "preprocess/format_converter/yuv_to_jpeg_converter.h"    // YUV 到 JPEG 转换
#include "common/log/logmanager.h"
extern "C" {
#include <libswscale/swscale.h>
}
// 已移除命名空间引用

VideoPipeline::VideoPipeline(boost::asio::io_context& io_ctx, const PipelineConfig& config)
    : config_(config)
    , io_ctx_(io_ctx)
{
    // 1. 创建拉流器
    puller_ = std::make_unique<ZlmHttpFlvPuller>(io_ctx_);
    puller_->SetReconnectParams(config_.puller.reconnect_delay, config_.puller.max_reconnect_attempts);
    
    // 2. 创建解码器
    decoder_ = std::make_unique<FfmpegDecoder>();
    decoder_->SetThreadCount(config_.decoder.decoder_threads);
    
    // 3. 创建预处理组件（可选）
    if (config_.preprocess.enable_preprocess) {
        converter_ = std::make_unique<OpenCVFormatConverter>();
    }
    
    // 4. 创建格式转换器（根据算法后端需求）
    std::string algo_type = config_.algorithm.getActiveAlgorithm();
    if (algo_type == "opencv") {
        // OpenCV 后端需要 YUV -> BGR 转换
        yuv_to_bgr_converter_ = std::make_unique<YuvToBgrConverter>();
        LOG_MAIN_INFO_AT("Created YUV to BGR converter for OpenCV backend");
    } else if (algo_type == "grpc") {
        // gRPC 后端需要 YUV -> JPEG 转换
        yuv_to_jpeg_converter_ = std::make_unique<YuvToJpegConverter>(85);  // quality=85
        LOG_MAIN_INFO_AT("Created YUV to JPEG converter for gRPC backend");
    }
    
    // 5. 初始化算法后端
    if (!initializeAlgorithmBackend()) {
        LOG_MAIN_ERROR_AT("Failed to initialize algorithm backend");
        // 不返回 false，允许无算法模式运行
    }
    
    // 6. 创建队列
    raw_queue_ = std::make_shared<RawPacketQueue>(config_.decoder.raw_queue_size);
    decoded_queue_ = std::make_shared<FrameDataQueue>(config_.decoder.decoded_queue_size);
    processed_queue_ = std::make_shared<FrameDataQueue>(config_.preprocess.processed_queue_size);
    
    LOG_MAIN_INFO_AT("VideoPipeline created: channel={}, url={}, algorithm={}", 
                    config_.channel_id, config_.puller.stream_url, algo_type);
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
    int cnt = 0;
    try {
        // 1. 启动拉流器（使用新的双回调接口）
        bool success = puller_->Start(config_.puller.stream_url,
            [this, &cnt](int codec_id, const uint8_t* data, int size) {
                /*++cnt;
                LOG_MAIN_DEBUG_AT("Sequence header received: size={}, count={}", size, cnt);*/
                onSequenceHeaderReceived(codec_id, data, size);
            },
            [this, &cnt](const uint8_t* data, int size, int64_t pts) {
                /*++cnt;
                LOG_MAIN_DEBUG_AT("NALU received: size={}, count={}", size, cnt);*/
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
                decoder_->Decode(packet.data.data(), static_cast<int>(packet.data.size()), packet.pts,
                    [this](VideoFrame&& frame) {
                        onFrameDecoded(std::move(frame));
                    });
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
    
    // 1. 停止算法后端
    if (algorithm_backend_) {
        algorithm_backend_->stop();
        LOG_MAIN_INFO_AT("Algorithm backend stopped");
    }
    
    // 2. 停止拉流器
    puller_->Stop();
    if (decoder_thread_.joinable()) {
        decoder_thread_.join();
    }
    
    // 3. 关闭解码器
    decoder_->Close();
    
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
            
            if (decoder_->Open(sps_pps_data_.data(), 
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
            
            if (decoder_->Open(sps_pps_h265_data_.data(), 
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
void VideoPipeline::onFrameDecoded(VideoFrame&& frame) {
    frames_decoded_++;
    //LOG_MAIN_DEBUG_AT("frames_decoded_: {}, {}x{}, {}", 
    //                     frames_decoded_.load(), 
    //                     frame.width, frame.height, frame.format);
    
    // 使用算法后端处理帧
    if (algorithm_backend_ && algorithm_backend_->isInitialized()) {
        std::string algo_type = config_.algorithm.getActiveAlgorithm();
        
        if (algo_type == "openvino") {
            // 路径 A: OpenVINO - 直接传入 YUV（零拷贝）
            algorithm_backend_->processFrame(frame);
            
        } else if (algo_type == "opencv") {
            // 路径 B: OpenCV - 先转换为 BGR，再处理
            if (yuv_to_bgr_converter_) {
                cv::Mat bgr = yuv_to_bgr_converter_->Convert(
                    frame.data[0], frame.data[1], frame.data[2],
                    frame.width, frame.height
                );
                
                if (!bgr.empty()) {
                    algorithm_backend_->processFrame(std::move(bgr), frame.pts);
                } else {
                    LOG_MAIN_WARN_AT("YUV to BGR conversion failed");
                }
            } else {
                LOG_MAIN_WARN_AT("OpenCV backend requires YuvToBgrConverter");
            }
            
        } else if (algo_type == "grpc") {
            // 路径 C: gRPC - 编码为 JPEG 并发送
            if (yuv_to_jpeg_converter_) {
                // 预分配缓冲区（500KB）
                static thread_local std::vector<uint8_t> jpeg_buffer(500 * 1024);
                
                size_t jpeg_size = yuv_to_jpeg_converter_->ConvertYuv420pZeroCopy(
                    frame.data[0], frame.data[1], frame.data[2],
                    frame.width, frame.height,
                    jpeg_buffer.data(),
                    jpeg_buffer.size()
                );
                
                if (jpeg_size > 0) {
                    // TODO: 调用 gRPC 后端发送
                    // algorithm_backend_->processFrame(...) 会在 Phase 4 实现
                    LOG_MAIN_DEBUG_AT("JPEG encoded: {} bytes", jpeg_size);
                } else {
                    LOG_MAIN_WARN_AT("YUV to JPEG conversion failed");
                }
            } else {
                LOG_MAIN_WARN_AT("gRPC backend requires YuvToJpegConverter");
            }
        }
    }
    
    // 如果启用了 OpenCV 格式转换器，转换为 cv::Mat（用于可视化或其他用途）
    if (converter_) {
        int64_t pts = frame.pts;
        converter_->Process(std::move(frame), [this, pts](cv::Mat&& mat, int64_t) {
            onFrameProcessed(std::move(mat), pts);
        });
    } else {
        // 没有处理器，直接输出 YUV 数据（可选：保存或传递给算法）
        LOG_MAIN_DEBUG_AT("Received decoded frame: {}x{} format={}", 
                         frame.width, frame.height, frame.format);
        
        // TODO: 如果需要直接使用 YUV 数据，在这里处理
        // 例如：传递给不需要 OpenCV 的算法模块
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

// ⚠️ 已弃用：旧的 gRPC 编码和发送功能
// 新架构使用算法后端（IAlgorithmBackend）替代
/*
/// @brief 编码并发送帧到 gRPC
void VideoPipeline::encodeAndSendToGrpc(const VideoFrame& frame) {
    if (!yuv_converter_) {
        return;
    }
    
    try {
        // 检查帧数据是否有效
        if (!frame.data[0] || !frame.data[1]) {
            LOG_MAIN_WARN_AT("Invalid YUV data pointers, skipping gRPC send");
            grpc_frames_failed_++;
            return;
        }

        if (frame.width <= 0 || frame.height <= 0) {
            LOG_MAIN_WARN_AT("Invalid frame dimensions {}x{}, skipping gRPC send",
                frame.width, frame.height);
            grpc_frames_failed_++;
            return;
        }

        // 支持的像素格式：YUV420P(0), NV12(12), NV21(13)
        bool supported_format = (frame.format == 0 ||   // AV_PIX_FMT_YUV420P
            frame.format == 12 ||  // AV_PIX_FMT_NV12
            frame.format == 13);   // AV_PIX_FMT_NV21

        if (!supported_format) {
            LOG_MAIN_WARN_AT("Unsupported pixel format: {}, skipping gRPC send", frame.format);
            grpc_frames_failed_++;
            return;
        }

        cv::Mat bgr_mat;

        // 根据像素格式选择不同的转换方式
        if (frame.format == 0) {
            // YUV420P: 三个独立平面
            bgr_mat = yuv_converter_->Convert(
                frame.data[0], frame.data[1], frame.data[2],
                frame.width, frame.height);
        }
        else if (frame.format == 12 || frame.format == 13) {
            // NV12/NV21: 使用 FFmpeg sws_scale 转换为 YUV420P，然后再用转换器            

            // 创建 SwsContext
            SwsContext* sws_ctx = sws_getContext(
                frame.width, frame.height,
                static_cast<AVPixelFormat>(frame.format),
                frame.width, frame.height,
                AV_PIX_FMT_YUV420P,
                SWS_BILINEAR, nullptr, nullptr, nullptr);

            if (!sws_ctx) {
                LOG_MAIN_WARN_AT("Failed to create SwsContext for format {}", frame.format);
                grpc_frames_failed_++;
                return;
            }

            // 分配目标帧（YUV420P）
            AVFrame* dst_frame = av_frame_alloc();
            dst_frame->format = AV_PIX_FMT_YUV420P;
            dst_frame->width = frame.width;
            dst_frame->height = frame.height;
            av_frame_get_buffer(dst_frame, 32);

            // 准备源数据指针
            const uint8_t* src_data[4] = {
                frame.data[0],
                frame.data[1],
                frame.data[2],
                nullptr
            };

            // 执行转换
            sws_scale(sws_ctx,
                src_data, frame.linesize,
                0, frame.height,
                dst_frame->data, dst_frame->linesize);

            // 使用现有的转换器将 YUV420P 转为 BGR
            bgr_mat = yuv_converter_->Convert(
                dst_frame->data[0], dst_frame->data[1], dst_frame->data[2],
                dst_frame->width, dst_frame->height);

            // 清理
            av_frame_free(&dst_frame);
            sws_freeContext(sws_ctx);
        }

        if (bgr_mat.empty()) {
            LOG_MAIN_WARN_AT("YUV to BGR conversion failed, skipping gRPC send");
            grpc_frames_failed_++;
            return;
        }
        
        // 编码为 JPEG
        auto jpeg_data = yuv_converter_->EncodeToJpeg(bgr_mat, 85);
        
        if (jpeg_data.empty()) {
            LOG_MAIN_WARN_AT("JPEG encoding failed, skipping gRPC send");
            grpc_frames_failed_++;
            return;
        }
        
        // 生成帧 ID
        std::string frame_id = "ch" + std::to_string(config_.channel_id) + 
                              "_frame" + std::to_string(frames_decoded_.load());
        
        // 获取时间戳
        int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // 发送到 gRPC
        if (grpc_sender_->sendFrame(jpeg_data, frame.width, frame.height, frame_id, timestamp)) {
            grpc_frames_sent_++;
            LOG_MAIN_DEBUG_AT("gRPC frame sent: {}x{}, size={} bytes", 
                             frame.width, frame.height, jpeg_data.size());
        } else {
            grpc_frames_failed_++;
            LOG_MAIN_WARN_AT("Failed to send frame via gRPC: {}", frame_id);
        }
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("encodeAndSendToGrpc failed: {}", e.what());
        grpc_frames_failed_++;
    }
}
*/

/// @brief 初始化算法后端
bool VideoPipeline::initializeAlgorithmBackend() {
    // 使用工厂创建算法后端
    algorithm_backend_ = AlgorithmBackendFactory::create(config_.algorithm);
    
    if (!algorithm_backend_) {
        LOG_MAIN_ERROR_AT("Failed to create algorithm backend");
        return false;
    }
    
    // 初始化后端
    if (!algorithm_backend_->initialize(config_.algorithm)) {
        LOG_MAIN_ERROR_AT("Failed to initialize algorithm backend: {}", 
                         algorithm_backend_->getBackendType());
        return false;
    }
    
    // 设置结果回调
    algorithm_backend_->setResultCallback([this](int channel_id, const DetectionResult& result) {
        LOG_MAIN_DEBUG_AT("Algorithm result received: channel={}, boxes={}, faces={}",
                         channel_id, result.boxes.size(), result.faces.size());
        // TODO: 处理检测结果（可以发送到队列或调用回调）
    });
    
    LOG_MAIN_INFO_AT("Algorithm backend initialized: type={}", 
                    algorithm_backend_->getBackendType());
    return true;
}

