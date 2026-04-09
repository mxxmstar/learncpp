#pragma once

#include <functional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

// 前向声明
namespace cv {
    class Mat;
}

namespace video_pipeline {
namespace algorithm_processor {

/**
 * @brief 检测框结构
 */
struct BoundingBox {
    float x = 0.0f;         // 左上角 X
    float y = 0.0f;         // 左上角 Y
    float width = 0.0f;     // 宽度
    float height = 0.0f;    // 高度
    std::string class_name; // 类别名称
    float confidence = 0.0f;// 置信度
    int class_id = 0;       // 类别 ID
};

/**
 * @brief 检测结果
 */
struct DetectionResult {
    std::string frame_id;                   // 帧 ID
    std::vector<BoundingBox> boxes;         // 检测框列表
    int64_t processing_time_ms = 0;         // 处理时间（毫秒）
    std::string algorithm;                  // 使用的算法
    std::map<std::string, std::string> metadata; // 额外元数据
};

/**
 * @brief 视频帧数据结构
 */
struct VideoFrame {
    std::vector<uint8_t> data;  // JPEG 编码的数据
    int width = 0;              // 宽度
    int height = 0;             // 高度
    std::string frame_id;       // 帧 ID
    int64_t timestamp = 0;      // 时间戳
};

/**
 * @brief 处理器统计信息
 */
struct ProcessorStats {
    int frames_processed = 0;       // 已处理的帧数
    int frames_failed = 0;          // 失败的帧数
    double avg_processing_time_ms = 0.0; // 平均处理时间
    double fps = 0.0;               // 处理帧率
};

/**
 * @brief 处理器类型枚举
 */
enum class ProcessorType {
    GRPC_PYTHON,    // Python gRPC 后端
    NATIVE_CPP      // 原生 C++ 后端（预留）
};

/**
 * @brief 处理器配置
 */
struct ProcessorConfig {
    ProcessorType type = ProcessorType::GRPC_PYTHON;
    
    // gRPC 配置
    std::string grpc_address = "localhost:50052";
    int grpc_target_fps = 10;
    
    // 原生 C++ 配置（预留）
    std::string model_path;
    std::string device = "cpu";
};

/**
 * @brief 视频处理器抽象接口
 * 
 * 设计目标：
 * 1. 解耦 VideoPipeline 和具体的处理实现
 * 2. 支持运行时切换不同的处理后端
 * 3. 为未来迁移到纯 C++ 算法预留扩展点
 */
class IAlgorithmProcessor {
public:
    virtual ~IAlgorithmProcessor() = default;
    
    /**
     * @brief 启动处理器
     * @return 成功返回 true
     */
    virtual bool Start() = 0;
    
    /**
     * @brief 停止处理器
     */
    virtual void Stop() = 0;
    
    /**
     * @brief 处理视频帧（异步）
     * @param frame 视频帧数据
     * @return 成功提交返回 true
     */
    virtual bool ProcessFrame(const VideoFrame& frame) = 0;
    
    /**
     * @brief 设置检测结果回调
     * @param callback 回调函数
     */
    using DetectionCallback = std::function<void(const DetectionResult&)>;
    virtual void SetDetectionCallback(DetectionCallback callback) = 0;
    
    /**
     * @brief 获取统计信息
     * @return 统计信息
     */
    virtual ProcessorStats GetStats() const = 0;
    
    /**
     * @brief 检查处理器是否可用
     * @return 可用返回 true
     */
    virtual bool IsAvailable() const = 0;
    
    /**
     * @brief 获取处理器类型
     * @return 处理器类型
     */
    virtual ProcessorType GetType() const = 0;
};

} // namespace algorithm_processor
} // namespace video_pipeline
