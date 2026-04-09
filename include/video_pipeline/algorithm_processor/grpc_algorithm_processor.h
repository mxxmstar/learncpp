#pragma once

#include "video_pipeline/algorithm_processor/i_algorithm_processor.h"
#include "video_grpc_client.h"
#include <mutex>
#include <atomic>
#include <chrono>

namespace video_pipeline {
namespace algorithm_processor {

/**
 * @brief 基于 gRPC 的算法处理器实现
 * 
 * 将 VideoGrpcClient 适配到 IAlgorithmProcessor 接口
 */
class GrpcAlgorithmProcessor : public IAlgorithmProcessor {
public:
    /**
     * @brief 构造函数
     * @param config 处理器配置
     */
    explicit GrpcAlgorithmProcessor(const ProcessorConfig& config);
    ~GrpcAlgorithmProcessor() override;
    
    // 禁止拷贝
    GrpcAlgorithmProcessor(const GrpcAlgorithmProcessor&) = delete;
    GrpcAlgorithmProcessor& operator=(const GrpcAlgorithmProcessor&) = delete;
    
    bool Start() override;
    void Stop() override;
    bool ProcessFrame(const VideoFrame& frame) override;
    void SetDetectionCallback(DetectionCallback callback) override;
    ProcessorStats GetStats() const override;
    bool IsAvailable() const override;
    ProcessorType GetType() const override { return ProcessorType::GRPC_PYTHON; }

private:
    ProcessorConfig config_;
    std::unique_ptr<grpc_module::VideoGrpcClient> grpc_client_;
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

} // namespace algorithm_processor
} // namespace video_pipeline
