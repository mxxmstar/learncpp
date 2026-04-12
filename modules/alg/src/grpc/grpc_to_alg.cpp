#include "alg/grpc/grpc_to_alg.h"
#include <iostream>



GrpcToAlg::GrpcToAlg(const ProcessorConfig& config)
    : config_(config)
    , last_frame_time_(std::chrono::steady_clock::now()) {
    
    std::cout << "[GrpcAlgorithmProcessor] Created with address: " 
              << config_.grpc_address << std::endl;
}

GrpcToAlg::~GrpcToAlg() {
    Stop();
}

bool GrpcToAlg::Start() {
    if (running_) {
        std::cerr << "[GrpcAlgorithmProcessor] Already running" << std::endl;
        return false;
    }
    
    try {
        // 创建 gRPC 客户端
        grpc_client_ = std::make_unique<grpc_module::VideoGrpcClient>(
            config_.grpc_address);
        
        // 连接服务器
        if (!grpc_client_->Connect()) {
            std::cerr << "[GrpcAlgorithmProcessor] Failed to connect to " 
                     << config_.grpc_address << std::endl;
            return false;
        }
        
        // 启动检测流
        auto callback = [this](const std::string& frame_id,
                              const std::vector<std::map<std::string, float>>& boxes,
                              int64_t processing_time_ms) {
            OnGrpcDetectionResult(frame_id, boxes, processing_time_ms);
        };
        
        if (!grpc_client_->StartDetectionStream(callback)) {
            std::cerr << "[GrpcAlgorithmProcessor] Failed to start detection stream" 
                     << std::endl;
            grpc_client_->Disconnect();
            return false;
        }
        
        running_ = true;
        std::cout << "[GrpcAlgorithmProcessor] Started successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[GrpcAlgorithmProcessor] Start failed: " << e.what() << std::endl;
        return false;
    }
}

void GrpcToAlg::Stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (grpc_client_) {
        grpc_client_->StopDetectionStream();
        grpc_client_->Disconnect();
        grpc_client_.reset();
    }
    
    std::cout << "[GrpcAlgorithmProcessor] Stopped" << std::endl;
}

bool GrpcToAlg::ProcessFrame(const VideoFrame& frame) {
    if (!running_ || !grpc_client_) {
        return false;
    }
    
    // 甯х巼鎺у埗
    if (!ShouldSendFrame()) {
        return true; // 跳过当前帧，但不算失败
    }
    
    try {
        // 发送帧到 gRPC 服务器进行检测
        bool success = grpc_client_->SendFrameForDetection(
            frame.data,
            frame.width,
            frame.height,
            frame.frame_id
        );
        
        if (!success) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.frames_failed++;
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[GrpcVideoProcessor] ProcessFrame error: " << e.what() 
                 << std::endl;
        
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.frames_failed++;
        return false;
    }
}

void GrpcToAlg::SetDetectionCallback(DetectionCallback callback) {
    detection_callback_ = callback;
}

ProcessorStats GrpcToAlg::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

bool GrpcToAlg::IsAvailable() const {
    return running_ && grpc_client_ && grpc_client_->IsConnected();
}

// ========== 私有方法 ==========

bool GrpcToAlg::ShouldSendFrame() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_frame_time_).count();
    
    // 计算目标帧间隔
    int target_interval_ms = 1000 / std::max(config_.grpc_target_fps, 1);
    
    if (elapsed < target_interval_ms) {
        // 还没到发送时间，跳过
        frame_skip_counter_++;
        return false;
    }
    
    // 更新上次发送时间
    last_frame_time_ = now;
    frame_skip_counter_ = 0;
    
    return true;
}

void GrpcToAlg::OnGrpcDetectionResult(
    const std::string& frame_id,
    const std::vector<std::map<std::string, float>>& boxes,
    int64_t processing_time_ms) {
    
    // 转换检测结果
    DetectionResult result;
    result.frame_id = frame_id;
    result.processing_time_ms = processing_time_ms;
    result.algorithm = "YOLOv5 (via gRPC)";
    
    for (const auto& box_data : boxes) {
        BoundingBox box;
        box.x = box_data.at("x");
        box.y = box_data.at("y");
        box.width = box_data.at("width");
        box.height = box_data.at("height");
        box.confidence = box_data.at("confidence");
        box.class_id = static_cast<int>(box_data.at("class_id"));
        box.class_name = "object_" + std::to_string(box.class_id); // 涓存椂鍚嶇О
        
        result.boxes.push_back(box);
    }
    
    // 更新统计信息
    UpdateStats(processing_time_ms);
    
    // 调用回调函数处理检测结果
    if (detection_callback_) {
        detection_callback_(result);
    }
}

void GrpcToAlg::UpdateStats(int64_t processing_time_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.frames_processed++;
    
    // 更新平均处理时间
    double total_time = stats_.avg_processing_time_ms * (stats_.frames_processed - 1) + 
                       processing_time_ms;
    stats_.avg_processing_time_ms = total_time / stats_.frames_processed;
    
    // 计算 FPS
    if (stats_.frames_processed > 1) {
        stats_.fps = 1000.0 / stats_.avg_processing_time_ms;
    }
}







