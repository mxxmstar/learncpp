#include "video_grpc_client.h"
#include "video_processing.grpc.pb.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace grpc_module {

struct VideoGrpcClient::Impl {
    // 用于存储额外的实现细节（如果需要）
};

VideoGrpcClient::VideoGrpcClient(const std::string& target)
    : impl_(std::make_unique<Impl>())
    , target_(target) {
}

VideoGrpcClient::~VideoGrpcClient() {
    Disconnect();
}

bool VideoGrpcClient::Connect(int timeout_seconds) {
    if (connected_) {
        return true;
    }

    try {
        // 创建 channel
        channel_ = grpc::CreateChannel(target_, grpc::InsecureChannelCredentials());
        
        // 等待连接就绪
        auto deadline = std::chrono::system_clock::now() + 
                       std::chrono::seconds(timeout_seconds);
        
        if (!channel_->WaitForConnected(deadline)) {
            std::cerr << "[VideoGrpcClient] Connection timeout to " << target_ << std::endl;
            return false;
        }
        
        connected_ = true;
        std::cout << "[VideoGrpcClient] Connected to " << target_ << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[VideoGrpcClient] Connection failed: " << e.what() << std::endl;
        return false;
    }
}

void VideoGrpcClient::Disconnect() {
    // 停止所有流
    StopDetectionStream();
    StopVideoProcessStream();
    
    if (channel_) {
        channel_.reset();
    }
    
    connected_ = false;
    std::cout << "[VideoGrpcClient] Disconnected" << std::endl;
}

// ========== 场景 1: 检测对象 ==========

bool VideoGrpcClient::StartDetectionStream(DetectionCallback callback) {
    if (!connected_) {
        std::cerr << "[VideoGrpcClient] Not connected" << std::endl;
        return false;
    }
    
    if (detection_running_) {
        std::cerr << "[VideoGrpcClient] Detection stream already running" << std::endl;
        return false;
    }
    
    try {
        // 创建 context
        detection_context_ = std::make_unique<grpc::ClientContext>();
        
        // 创建 stub 并启动双向流
        auto stub = video_processing::VideoProcessingService::NewStub(channel_);
        detection_stream_ = stub->DetectObjects(detection_context_.get());
        
        if (!detection_stream_) {
            std::cerr << "[VideoGrpcClient] Failed to start detection stream" << std::endl;
            return false;
        }
        
        // 保存回调
        detection_callback_ = callback;
        detection_running_ = true;
        
        // 启动读取线程
        detection_thread_ = std::thread(&VideoGrpcClient::DetectionStreamWorker, this);
        
        std::cout << "[VideoGrpcClient] Detection stream started" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[VideoGrpcClient] Start detection stream failed: " << e.what() << std::endl;
        detection_running_ = false;
        return false;
    }
}

bool VideoGrpcClient::SendFrameForDetection(const std::vector<uint8_t>& frame_data,
                                            int width,
                                            int height,
                                            const std::string& frame_id) {
    if (!detection_running_) {
        std::cerr << "[VideoGrpcClient] Detection stream not running" << std::endl;
        return false;
    }
    
    try {
        // 编码帧
        auto video_frame = EncodeFrame(frame_data, width, height, frame_id);
        
        // 发送
        if (!detection_stream_->Write(video_frame)) {
            std::cerr << "[VideoGrpcClient] Failed to write frame" << std::endl;
            return false;
        }
        
        // 更新统计
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.frames_sent++;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[VideoGrpcClient] Send frame failed: " << e.what() << std::endl;
        return false;
    }
}

void VideoGrpcClient::StopDetectionStream() {
    if (!detection_running_) {
        return;
    }
    
    detection_running_ = false;
    
    // 关闭写入端
    if (detection_stream_) {
        detection_stream_->WritesDone();
    }
    
    // 等待读取线程结束
    if (detection_thread_.joinable()) {
        detection_thread_.join();
    }
    
    // 清理资源
    detection_context_.reset();
    detection_stream_.reset();
    detection_callback_ = nullptr;
    
    std::cout << "[VideoGrpcClient] Detection stream stopped" << std::endl;
}

void VideoGrpcClient::DetectionStreamWorker() {
    std::cout << "[VideoGrpcClient] Detection worker started" << std::endl;
    
    try {
        video_processing::DetectionResult result;
        
        while (detection_running_ && detection_stream_->Read(&result)) {
            // 解析检测结果
            std::vector<std::map<std::string, float>> boxes;
            
            for (const auto& box : result.boxes()) {
                std::map<std::string, float> box_data;
                box_data["x"] = box.x();
                box_data["y"] = box.y();
                box_data["width"] = box.width();
                box_data["height"] = box.height();
                box_data["confidence"] = box.confidence();
                box_data["class_id"] = static_cast<float>(box.class_id());
                boxes.push_back(box_data);
            }
            
            // 调用回调
            if (detection_callback_) {
                detection_callback_(result.frame_id(), boxes, result.processing_time_ms());
            }
            
            // 更新统计
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.frames_received++;
                
                // 计算平均延迟
                if (stats_.frames_received > 0) {
                    double total_latency = stats_.avg_latency_ms * (stats_.frames_received - 1) + 
                                          result.processing_time_ms();
                    stats_.avg_latency_ms = total_latency / stats_.frames_received;
                }
            }
        }
        
        // 检查最终状态
        grpc::Status status = detection_stream_->Finish();
        if (!status.ok()) {
            std::cerr << "[VideoGrpcClient] Detection stream error: " 
                     << status.error_message() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[VideoGrpcClient] Detection worker exception: " << e.what() << std::endl;
    }
    
    std::cout << "[VideoGrpcClient] Detection worker stopped" << std::endl;
}

// ========== 场景 2: 处理并返回视频 ==========

bool VideoGrpcClient::StartVideoProcessStream(ProcessedFrameCallback callback) {
    if (!connected_) {
        std::cerr << "[VideoGrpcClient] Not connected" << std::endl;
        return false;
    }
    
    if (video_running_) {
        std::cerr << "[VideoGrpcClient] Video process stream already running" << std::endl;
        return false;
    }
    
    try {
        // 创建 context
        video_context_ = std::make_unique<grpc::ClientContext>();
        
        // 创建 stub 并启动双向流
        auto stub = video_processing::VideoProcessingService::NewStub(channel_);
        video_stream_ = stub->ProcessAndReturnVideo(video_context_.get());
        
        if (!video_stream_) {
            std::cerr << "[VideoGrpcClient] Failed to start video process stream" << std::endl;
            return false;
        }
        
        // 保存回调
        video_callback_ = callback;
        video_running_ = true;
        
        // 启动读取线程
        video_thread_ = std::thread(&VideoGrpcClient::VideoProcessStreamWorker, this);
        
        std::cout << "[VideoGrpcClient] Video process stream started" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[VideoGrpcClient] Start video process stream failed: " << e.what() << std::endl;
        video_running_ = false;
        return false;
    }
}

bool VideoGrpcClient::SendFrameForProcessing(const std::vector<uint8_t>& frame_data,
                                             int width,
                                             int height,
                                             const std::string& frame_id) {
    if (!video_running_) {
        std::cerr << "[VideoGrpcClient] Video process stream not running" << std::endl;
        return false;
    }
    
    try {
        // 编码帧
        auto video_frame = EncodeFrame(frame_data, width, height, frame_id);
        
        // 发送
        if (!video_stream_->Write(video_frame)) {
            std::cerr << "[VideoGrpcClient] Failed to write frame" << std::endl;
            return false;
        }
        
        // 更新统计
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.frames_sent++;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[VideoGrpcClient] Send frame failed: " << e.what() << std::endl;
        return false;
    }
}

void VideoGrpcClient::StopVideoProcessStream() {
    if (!video_running_) {
        return;
    }
    
    video_running_ = false;
    
    // 关闭写入端
    if (video_stream_) {
        video_stream_->WritesDone();
    }
    
    // 等待读取线程结束
    if (video_thread_.joinable()) {
        video_thread_.join();
    }
    
    // 清理资源
    video_context_.reset();
    video_stream_.reset();
    video_callback_ = nullptr;
    
    std::cout << "[VideoGrpcClient] Video process stream stopped" << std::endl;
}

void VideoGrpcClient::VideoProcessStreamWorker() {
    std::cout << "[VideoGrpcClient] Video process worker started" << std::endl;
    
    try {
        video_processing::ProcessedFrame result;
        
        while (video_running_ && video_stream_->Read(&result)) {
            // 解码视频帧
            ProcessedFrameData frame_data(result.data().begin(), result.data().end());
            
            // 调用回调
            if (video_callback_) {
                video_callback_(result.frame_id(), frame_data, 
                               result.width(), result.height(),
                               result.processing_time_ms());
            }
            
            // 更新统计
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.frames_received++;
                
                // 计算平均延迟
                if (stats_.frames_received > 0) {
                    double total_latency = stats_.avg_latency_ms * (stats_.frames_received - 1) + 
                                          result.processing_time_ms();
                    stats_.avg_latency_ms = total_latency / stats_.frames_received;
                }
            }
        }
        
        // 检查最终状态
        grpc::Status status = video_stream_->Finish();
        if (!status.ok()) {
            std::cerr << "[VideoGrpcClient] Video process stream error: " 
                     << status.error_message() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[VideoGrpcClient] Video process worker exception: " << e.what() << std::endl;
    }
    
    std::cout << "[VideoGrpcClient] Video process worker stopped" << std::endl;
}

// ========== 辅助方法 ==========

VideoGrpcClient::Statistics VideoGrpcClient::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

std::string VideoGrpcClient::GenerateFrameId() {
    int id = frame_counter_++;
    
    std::ostringstream oss;
    oss << "frame_" << std::setw(6) << std::setfill('0') << id;
    return oss.str();
}

video_processing::VideoFrame VideoGrpcClient::EncodeFrame(const std::vector<uint8_t>& frame_data,
                                                          int width,
                                                          int height,
                                                          const std::string& frame_id) {
    video_processing::VideoFrame video_frame;
    
    // 直接使用传入的 JPEG 数据
    video_frame.set_data(frame_data.data(), frame_data.size());
    video_frame.set_width(width);
    video_frame.set_height(height);
    video_frame.set_format(1); // BGR/JPEG
    
    // 时间戳
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    video_frame.set_timestamp(ms);
    
    // 帧ID
    if (frame_id.empty()) {
        video_frame.set_frame_id(GenerateFrameId());
    } else {
        video_frame.set_frame_id(frame_id);
    }
    
    return video_frame;
}

} // namespace grpc_module
