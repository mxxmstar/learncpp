#pragma once

#include "decoder/i_decoder.h"  // VideoFrame
#include "videopipeline/pipeline_config.h"
#include <opencv2/opencv.hpp>
#include <functional>
#include <memory>

/// @brief 检测结果（简化版，可根据需要扩展）
struct DetectionResult {
    int channel_id = 0;
    int64_t timestamp = 0;
    
    // 目标检测框
    struct BoundingBox {
        float x, y, width, height;
        float confidence;
        int class_id;
        std::string class_name;
    };
    
    std::vector<BoundingBox> boxes;
    
    // 人脸检测
    struct Face {
        float x, y, width, height;
        float confidence;
    };
    std::vector<Face> faces;
    
    // 其他自定义数据
    std::map<std::string, std::any> metadata;
};

/// @brief 算法后端结果回调
using AlgorithmResultCallback = std::function<void(int channel_id, const DetectionResult& result)>;

/// @brief 算法后端接口（策略模式）
/// 
/// 支持三种实现：
/// 1. OpenVINOBackend - 本地 OpenVINO 推理（零拷贝）
/// 2. OpenCVBackend - 本地 OpenCV 算法（需要预处理）
/// 3. GrpcBackend - 远程 gRPC 算法（JPEG 编码）
class IAlgorithmBackend {
public:
    virtual ~IAlgorithmBackend() = default;
    
    /// @brief 初始化算法后端
    /// @param config 算法配置
    /// @return true 成功，false 失败
    virtual bool initialize(const AlgorithmConfig& config) = 0;
    
    /// @brief 处理帧（YUV 原始数据）
    /// @param frame 解码后的视频帧（YUV 格式）
    /// @note OpenVINO 后端优先使用此方法（零拷贝）
    virtual void processFrame(const VideoFrame& frame) = 0;
    
    /// @brief 处理帧（BGR Mat）
    /// @param frame BGR 格式的图像
    /// @param pts 时间戳
    /// @note OpenCV 后端优先使用此方法
    virtual void processFrame(cv::Mat&& frame, int64_t pts) = 0;
    
    /// @brief 设置结果回调
    /// @param callback 结果回调函数
    void setResultCallback(AlgorithmResultCallback callback) {
        result_callback_ = std::move(callback);
    }
    
    /// @brief 获取后端类型
    /// @return "openvino", "opencv", "grpc", 或 "none"
    virtual std::string getBackendType() const = 0;
    
    /// @brief 是否已初始化
    virtual bool isInitialized() const = 0;
    
    /// @brief 停止后端（释放资源）
    virtual void stop() = 0;
    
protected:
    /// @brief 结果回调
    AlgorithmResultCallback result_callback_;
};
