#pragma once

#include <grpcpp/grpcpp.h>
#include <string>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <memory>
#include <vector>
#include <map>

// 前向声明 OpenCV
namespace cv {
    class Mat;
}

namespace video_processing {
    class VideoProcessingService;
    class VideoFrame;
    class DetectionResult;
    class ProcessedFrame;
}

namespace grpc_module {

// 回调类型定义
using DetectionCallback = std::function<void(const std::string& frame_id, 
                                             const std::vector<std::map<std::string, float>>& boxes,
                                             int64_t processing_time_ms)>;

using ProcessedFrameData = std::vector<uint8_t>; // JPEG 编码的数据
using ProcessedFrameCallback = std::function<void(const std::string& frame_id,
                                                  const ProcessedFrameData& frame_data,
                                                  int width,
                                                  int height,
                                                  int64_t processing_time_ms)>;

/**
 * @brief 视频处理 gRPC 客户端
 * 
 * 支持双向流式通信：
 * - 场景 1: 发送视频帧，接收检测结果
 * - 场景 2: 发送视频帧，接收处理后的视频
 */
class VideoGrpcClient {
public:
    /**
     * @brief 构造函数
     * @param target gRPC 服务器地址，例如 "localhost:50052"
     */
    explicit VideoGrpcClient(const std::string& target = "localhost:50052");
    ~VideoGrpcClient();

    // 禁止拷贝
    VideoGrpcClient(const VideoGrpcClient&) = delete;
    VideoGrpcClient& operator=(const VideoGrpcClient&) = delete;

    /**
     * @brief 连接到服务器
     * @return 成功返回 true
     */
    bool Connect(int timeout_seconds = 5);

    /**
     * @brief 断开连接
     */
    void Disconnect();

    /**
     * @brief 是否已连接
     */
    bool IsConnected() const { return connected_; }

    // ========== 场景 1: 检测对象（返回元数据）==========

    /**
     * @brief 启动检测流
     * @param callback 检测结果回调函数
     * @return 成功返回 true
     */
    bool StartDetectionStream(DetectionCallback callback);

    /**
     * @brief 发送视频帧进行检测
     * @param frame 视频帧数据（JPEG 编码的 bytes）
     * @param width 宽度
     * @param height 高度
     * @param frame_id 帧ID（可选，用于匹配响应）
     * @return 成功返回 true
     */
    bool SendFrameForDetection(const std::vector<uint8_t>& frame_data,
                               int width,
                               int height,
                               const std::string& frame_id = "");

    /**
     * @brief 停止检测流
     */
    void StopDetectionStream();

    // ========== 场景 2: 处理并返回视频（返回图像）==========

    /**
     * @brief 启动视频处理流
     * @param callback 处理后视频帧的回调函数
     * @return 成功返回 true
     */
    bool StartVideoProcessStream(ProcessedFrameCallback callback);

    /**
     * @brief 发送视频帧进行处理
     * @param frame 视频帧数据（JPEG 编码的 bytes）
     * @param width 宽度
     * @param height 高度
     * @param frame_id 帧ID
     * @return 成功返回 true
     */
    bool SendFrameForProcessing(const std::vector<uint8_t>& frame_data,
                                int width,
                                int height,
                                const std::string& frame_id = "");

    /**
     * @brief 停止视频处理流
     */
    void StopVideoProcessStream();

    /**
     * @brief 获取统计信息
     */
    struct Statistics {
        int frames_sent = 0;
        int frames_received = 0;
        double avg_latency_ms = 0.0;
    };
    
    Statistics GetStatistics() const;

private:
    // 内部实现
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    std::string target_;
    std::shared_ptr<grpc::Channel> channel_;
    std::atomic<bool> connected_{false};
    
    // 检测流
    std::unique_ptr<grpc::ClientContext> detection_context_;
    std::unique_ptr<grpc::ClientReaderWriter<video_processing::VideoFrame, video_processing::DetectionResult>> detection_stream_;
    std::thread detection_thread_;
    DetectionCallback detection_callback_;
    std::atomic<bool> detection_running_{false};
    
    // 视频处理流
    std::unique_ptr<grpc::ClientContext> video_context_;
    std::unique_ptr<grpc::ClientReaderWriter<video_processing::VideoFrame, video_processing::ProcessedFrame>> video_stream_;
    std::thread video_thread_;
    ProcessedFrameCallback video_callback_;
    std::atomic<bool> video_running_{false};
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    Statistics stats_;
    
    // 帧计数器
    std::atomic<int> frame_counter_{0};
    
    // 私有方法
    void DetectionStreamWorker();
    void VideoProcessStreamWorker();
    std::string GenerateFrameId();
    video_processing::VideoFrame EncodeFrame(const std::vector<uint8_t>& frame_data,
                                             int width,
                                             int height,
                                             const std::string& frame_id);
};

} // namespace grpc_module
