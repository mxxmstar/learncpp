#pragma once

#include <string>
#include <vector>
#include <cstdint>

/// @brief 视频流水线配置
struct PipelineConfig {
    /// @brief 通道 ID（0 表示自动生成）
    int channel_id = 0;
    
    /// @brief 流 URL（RTSP/RTMP/HTTP-FLV）
    std::string stream_url;
    
    // ==================== 拉流配置 ====================
    /// @brief 重连延迟（秒）
    int reconnect_delay = 3;
    
    /// @brief 最大重连次数（-1 表示无限重试）
    int max_reconnect_attempts = -1;
    
    /// @brief 拉流超时时间（毫秒）
    int pull_timeout_ms = 5000;
    
    // ==================== 解码配置 ====================
    /// @brief 解码线程数
    int decoder_threads = 2;
    
    /// @brief 输出像素格式（默认 BGR24）
    int output_format = 0;  // AV_PIX_FMT_BGR24
    
    // ==================== 处理配置 ====================
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
    
    // ==================== 队列配置 ====================
    /// @brief 原始数据包队列大小
    int raw_queue_size = 64;
    
    /// @brief 解码帧队列大小
    int decoded_queue_size = 16;
    
    /// @brief 处理帧队列大小
    int processed_queue_size = 16;
    
    // ==================== 算法配置 ====================
    /// @brief 算法类型
    /// - "none": 无算法
    /// - "yolo_v5": YOLOv5 目标检测
    /// - "yolo_v8": YOLOv8 目标检测
    /// - "face_detect": 人脸检测
    /// - "custom": 自定义算法
    std::string algorithm_type = "none";
    
    /// @brief 算法模型路径
    std::string model_path;
    
    /// @brief 算法置信度阈值
    float confidence_threshold = 0.5f;
    
    // ==================== gRPC 配置 ====================
    /// @brief 是否启用 gRPC 视频发送
    bool enable_grpc_send = false;
    
    /// @brief gRPC 服务器地址
    std::string grpc_server_address = "localhost:50053";
    
    /// @brief gRPC 目标帧率（每秒发送的帧数）
    int grpc_target_fps = 10;
    
    // ==================== 其他配置 ====================
    /// @brief 是否保存原始数据（用于调试）
    bool save_raw_data = false;
    
    /// @brief 保存路径
    std::string save_path = "./recordings";
    
    /// @brief 日志级别
    int log_level = 2;  // 0=debug, 1=info, 2=warn, 3=error
    
    /// @brief 验证配置是否有效
    bool isValid() const {
        return !stream_url.empty();
    }
    
    /// @brief 获取默认的 RTSP URL
    static std::string getDefaultRtspUrl(const std::string& host, 
                                        const std::string& app,
                                        const std::string& stream) {
        return "rtsp://" + host + "/" + app + "/" + stream;
    }
};
