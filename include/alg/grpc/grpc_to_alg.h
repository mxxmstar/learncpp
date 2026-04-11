#pragma once

#include "alg/grpc/i_algorithm_processor.h"
#include "video_grpc_client.h"  // GrpcToAlg 直接使用 VideoGrpcClient
#include <mutex>
#include <atomic>
#include <chrono>

/**
 * @brief 基于 gRPC 的算法处理器实现
 * 
 * 封装 VideoGrpcClient，提供统一的 IAlgorithmProcessor 接口
 */
class GrpcToAlg : public IAlgorithmProcessor {
public:
    /**
     * @brief 构造函数
     * @param config 处理器配置
     */
    explicit GrpcToAlg(const ProcessorConfig& config);
    ~GrpcToAlg() override;
    
    // 禁止拷贝
    GrpcToAlg(const GrpcToAlg&) = delete;
    GrpcToAlg& operator=(const GrpcToAlg&) = delete;
    
    bool Start() override;
    void Stop() override;
    bool ProcessFrame(const VideoFrame& frame) override;
    void SetDetectionCallback(DetectionCallback callback) override;
    ProcessorStats GetStats() const override;
    bool IsAvailable() const override;
    ProcessorType GetType() const override { return ProcessorType::GRPC_PYTHON; }

private:
    ProcessorConfig config_;
    std::unique_ptr<grpc_module::VideoGrpcClient> grpc_client_;  // 直接使用 VideoGrpcClient
    DetectionCallback detection_callback_;
    
    mutable std::mutex stats_mutex_;
    ProcessorStats stats_;
    
    std::atomic<bool> running_{false};
    std::atomic<int> frame_counter_{0};
    
    // 帧率控制
    std::chrono::steady_clock::time_point last_frame_time_;
    int frame_skip_counter_ = 0;
    
    /**
     * @brief 检查是否应该发送这一帧（帧率控制）
     */
    bool ShouldSendFrame();
    
    /**
     * @brief gRPC 检测结果回调
     */
    void OnGrpcDetectionResult(
        const std::string& frame_id,
        const std::vector<std::map<std::string, float>>& boxes,
        int64_t processing_time_ms
    );
    
    /**
     * @brief 更新统计信息
     */
    void UpdateStats(int64_t processing_time_ms);
};
