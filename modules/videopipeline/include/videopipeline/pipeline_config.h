#pragma once

#include <string>
#include <vector>
#include <cstdint>

/// @brief 拉流配置
struct PullerConfig {
    /// @brief 流 URL（RTSP/RTMP/HTTP-FLV）
    std::string stream_url;
    
    /// @brief 重连延迟（秒）
    int reconnect_delay = 3;
    
    /// @brief 最大重连次数（-1 表示无限重试）
    int max_reconnect_attempts = -1;
    
    /// @brief 拉流超时时间（毫秒）
    int pull_timeout_ms = 5000;
    
    /// @brief 验证配置是否有效
    bool isValid() const {
        return !stream_url.empty();
    }
};

/// @brief 解码配置
struct DecoderConfig {
    /// @brief 解码线程数
    int decoder_threads = 2;
    
    /// @brief 输出像素格式（默认 BGR24）
    int output_format = 0;  // AV_PIX_FMT_BGR24
    
    /// @brief 原始数据包队列大小
    int raw_queue_size = 64;
    
    /// @brief 解码帧队列大小
    int decoded_queue_size = 16;
};

/// @brief 预处理配置
/// @note 目前仅 opencv 模块使用
struct PreprocessConfig {
    /// @brief 是否启用预处理
    bool enable_preprocess = true;
    
    /// @brief 滤镜列表（按顺序应用）
    /// 支持的滤镜：
    /// - "gaussian_blur": 高斯模糊
    /// - "histogram_eq": 直方图均衡化
    /// - "canny_edge": Canny 边缘检测
    /// - "resize": 缩放（需要指定 target_width/target_height）
    std::vector<std::string> filters;
    
    /// @brief 目标宽度（用于 resize 滤镜）
    int target_width = 0;
    
    /// @brief 目标高度（用于 resize 滤镜）
    int target_height = 0;
    
    /// @brief 处理帧队列大小
    int processed_queue_size = 16;
};

/// @brief gRPC 远程算法配置
struct GrpcAlgorithmConfig {
    /// @brief 是否启用 gRPC 视频发送
    bool enabled = false;
    
    /// @brief gRPC 服务器地址
    std::string server_address = "localhost:50053";
    
    /// @brief gRPC 目标帧率（每秒发送的帧数）
    int target_fps = 10;
    
    /// @brief 验证配置是否有效
    bool isValid() const {
        return enabled && !server_address.empty();
    }
};

/// @brief OpenVINO 本地算法配置
struct OpenVINOAlgorithmConfig {
    /// @brief 是否启用 OpenVINO 推理
    bool enabled = false;
    
    /// @brief 模型路径（.xml 文件）
    std::string model_path;
    
    /// @brief 设备类型（CPU/GPU/MYRIAD等）
    std::string device = "CPU";
    
    /// @brief 置信度阈值
    float confidence_threshold = 0.5f;
    
    /// @brief 批处理大小
    int batch_size = 1;
    
    /// @brief 验证配置是否有效
    bool isValid() const {
        return enabled && !model_path.empty();
    }
};

/// @brief OpenCV 本地算法配置
struct OpenCVAlgorithmConfig {
    /// @brief 是否启用 OpenCV 算法
    bool enabled = false;
    
    /// @brief 算法类型
    /// - "face_detect": 人脸检测
    /// - "object_track": 目标跟踪
    /// - "motion_detect": 运动检测
    /// - "custom": 自定义算法
    std::string algorithm_type = "none";
    
    /// @brief 配置文件路径（如 cascade XML）
    std::string config_path;
    
    /// @brief 置信度阈值
    float confidence_threshold = 0.5f;
    
    /// @brief 验证配置是否有效
    bool isValid() const {
        return enabled && algorithm_type != "none";
    }
};

/// @brief 算法配置（支持多种算法后端）
struct AlgorithmConfig {
    /// @brief gRPC 远程算法配置
    GrpcAlgorithmConfig grpc;
    
    /// @brief OpenVINO 本地算法配置
    OpenVINOAlgorithmConfig openvino;
    
    /// @brief OpenCV 本地算法配置
    OpenCVAlgorithmConfig opencv;
    
    /// @brief 获取当前启用的算法类型
    /// @return "grpc", "openvino", "opencv", 或 "none"
    std::string getActiveAlgorithm() const {
        if (grpc.enabled) return "grpc";
        if (openvino.enabled) return "openvino";
        if (opencv.enabled) return "opencv";
        return "none";
    }
    
    /// @brief 是否有启用的算法
    bool hasAlgorithm() const {
        return grpc.enabled || openvino.enabled || opencv.enabled;
    }
};

/// @brief 录制配置
struct RecordingConfig {
    /// @brief 是否保存原始数据（用于调试）
    bool save_raw_data = false;
    
    /// @brief 保存路径
    std::string save_path = "./recordings";
};

/// @brief 日志配置
struct LogConfig {
    /// @brief 日志级别
    /// - 0: DEBUG
    /// - 1: INFO
    /// - 2: WARN
    /// - 3: ERROR
    int log_level = 2;
};

/// @brief 视频流水线配置
struct PipelineConfig {
    /// @brief 通道 ID（0 表示自动生成）
    int channel_id = 0;
    
    // ==================== 子配置 ====================
    /// @brief 拉流配置
    PullerConfig puller;
    
    /// @brief 解码配置
    DecoderConfig decoder;
    
    /// @brief 预处理配置
    PreprocessConfig preprocess;
    
    /// @brief 算法配置（gRPC/OpenVINO/OpenCV）
    AlgorithmConfig algorithm;
    
    /// @brief 录制配置
    RecordingConfig recording;
    
    /// @brief 日志配置
    LogConfig log;
    
    // ==================== 便捷方法 ====================
    /// @brief 验证配置是否有效
    bool isValid() const {
        return puller.isValid();
    }
    
    /// @brief 获取默认的 RTSP URL
    static std::string getDefaultRtspUrl(const std::string& host, 
                                        const std::string& app,
                                        const std::string& stream) {
        return "rtsp://" + host + "/" + app + "/" + stream;
    }
    
    /// @brief 快速创建 gRPC 算法配置
    static PipelineConfig createWithGrpc(const std::string& stream_url,
                                        const std::string& grpc_server = "localhost:50053",
                                        int fps = 10) {
        PipelineConfig config;
        config.puller.stream_url = stream_url;
        config.algorithm.grpc.enabled = true;
        config.algorithm.grpc.server_address = grpc_server;
        config.algorithm.grpc.target_fps = fps;
        return config;
    }
    
    /// @brief 快速创建 OpenVINO 算法配置
    static PipelineConfig createWithOpenVINO(const std::string& stream_url,
                                            const std::string& model_path,
                                            const std::string& device = "CPU",
                                            float confidence = 0.5f) {
        PipelineConfig config;
        config.puller.stream_url = stream_url;
        config.algorithm.openvino.enabled = true;
        config.algorithm.openvino.model_path = model_path;
        config.algorithm.openvino.device = device;
        config.algorithm.openvino.confidence_threshold = confidence;
        return config;
    }
    
    /// @brief 快速创建 OpenCV 算法配置
    static PipelineConfig createWithOpenCV(const std::string& stream_url,
                                          const std::string& algorithm_type,
                                          const std::string& config_path = "",
                                          float confidence = 0.5f) {
        PipelineConfig config;
        config.puller.stream_url = stream_url;
        config.algorithm.opencv.enabled = true;
        config.algorithm.opencv.algorithm_type = algorithm_type;
        config.algorithm.opencv.config_path = config_path;
        config.algorithm.opencv.confidence_threshold = confidence;
        return config;
    }
};
