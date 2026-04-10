#include "video_pipeline/grpc_video_sender.h"
#include "log/logmanager.h"
#include <iostream>

namespace video_pipeline {

GrpcVideoSender::GrpcVideoSender(const std::string& server_address)
    : server_address_(server_address) {
    
    LOG_MAIN_INFO_AT("[GrpcVideoSender] Created with address: {}", server_address_);
}

GrpcVideoSender::~GrpcVideoSender() {
    stop();
    LOG_MAIN_INFO_AT("[GrpcVideoSender] Destroyed");
}

bool GrpcVideoSender::start() {
    if (running_) {
        LOG_MAIN_WARN_AT("[GrpcVideoSender] Already running");
        return false;
    }
    
    try {
        // 创建 gRPC 客户端
        grpc_client_ = std::make_unique<grpc_module::VideoGrpcClient>(server_address_);
        
        // 连接服务器
        if (!grpc_client_->Connect()) {
            LOG_MAIN_ERROR_AT("[GrpcVideoSender] Failed to connect to {}", server_address_);
            return false;
        }
        
        // 启动检测流（使用空回调，第一阶段不处理返回结果）
        auto callback = [](const std::string& frame_id,
                          const std::vector<std::map<std::string, float>>& boxes,
                          int64_t processing_time_ms) {
            // 第一阶段：仅打印日志
            LOG_MAIN_DEBUG_AT("[GrpcVideoSender] Received detection result for frame: {}, boxes: {}", 
                             frame_id, boxes.size());
        };
        
        if (!grpc_client_->StartDetectionStream(callback)) {
            LOG_MAIN_ERROR_AT("[GrpcVideoSender] Failed to start detection stream");
            grpc_client_->Disconnect();
            return false;
        }
        
        running_ = true;
        LOG_MAIN_INFO_AT("[GrpcVideoSender] Started successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("[GrpcVideoSender] Start failed: {}", e.what());
        return false;
    }
}

void GrpcVideoSender::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (grpc_client_) {
        grpc_client_->StopDetectionStream();
        grpc_client_->Disconnect();
        grpc_client_.reset();
    }
    
    LOG_MAIN_INFO_AT("[GrpcVideoSender] Stopped");
}

bool GrpcVideoSender::sendFrame(const std::vector<uint8_t>& jpeg_data,
                                 int width,
                                 int height,
                                 const std::string& frame_id,
                                 int64_t timestamp) {
    if (!running_ || !grpc_client_) {
        LOG_MAIN_WARN_AT("[GrpcVideoSender] Not connected, cannot send frame");
        return false;
    }
    
    try {
        // 发送帧到检测流（注意：SendFrameForDetection 不接受 timestamp 参数）
        bool success = grpc_client_->SendFrameForDetection(
            jpeg_data, width, height, frame_id);
        
        if (!success) {
            LOG_MAIN_WARN_AT("[GrpcVideoSender] Failed to send frame: {}", frame_id);
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("[GrpcVideoSender] Send frame failed: {}", e.what());
        return false;
    }
}

bool GrpcVideoSender::isConnected() const {
    return running_ && grpc_client_ && grpc_client_->IsConnected();
}

} // namespace video_pipeline
