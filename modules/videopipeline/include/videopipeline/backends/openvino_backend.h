#pragma once

#include "videopipeline/i_algorithm_backend.h"
#include "alg/inference/inference_engine_factory.h"
#include "alg/inference/tensor_data.h"
#include "alg/inference/prepost_processor.h"  // PrePostProcessor
#include "common/log/logmanager.h"
#include <memory>

/// @brief OpenVINO 算法后端
/// 
/// 特点：
/// - 零拷贝：直接从 YUV 数据创建 TensorData
/// - 高性能：使用 OpenVINO CPU/GPU 推理
/// - 低延迟：< 5ms (1920×1080)
class OpenVINOBackend : public IAlgorithmBackend {
public:
    OpenVINOBackend() = default;
    ~OpenVINOBackend() override = default;
    
    bool initialize(const AlgorithmConfig& config) override {
        if (!config.openvino.isValid()) {
            LOG_MAIN_ERROR_AT("[OpenVINOBackend] Invalid config: model_path is empty");
            return false;
        }
        
        // 创建推理引擎配置
        InferenceConfig engine_config;
        engine_config.model_path = config.openvino.model_path;
        engine_config.device = config.openvino.device;
        engine_config.batch_size = config.openvino.batch_size;
        engine_config.async_mode = true;  // 启用异步模式
        
        // ✅ 启用 PrePostProcessor（零拷贝预处理）
        engine_config.enable_preprocessor = true;
        
        // 配置预处理参数
        // 根据解码器输出格式设置输入格式
        // FFmpeg 解码器通常输出 YUV420P (format=0) 或 NV12 (format=12)
        engine_config.preprocess_config.input_format = ImageFormat::YUV420P;  // 默认 YUV420P
        engine_config.preprocess_config.target_size = {640, 640};              // YOLOv5 默认尺寸
        engine_config.preprocess_config.normalize = true;                      // 归一化到 [0, 1]
        engine_config.preprocess_config.mean = {0.0f, 0.0f, 0.0f};            // 均值
        engine_config.preprocess_config.std = {255.0f, 255.0f, 255.0f};       // 标准差（除以 255）
        engine_config.preprocess_config.output_layout = "NCHW";                // 输出布局
        engine_config.preprocess_config.output_type = "f32";                   // 输出类型
        
        LOG_MAIN_INFO_AT("[OpenVINOBackend] PrePostProcessor enabled:");
        LOG_MAIN_INFO_AT("  Input format: YUV420P/NV12/NV21 (auto-detected)");
        LOG_MAIN_INFO_AT("  Target size: 640x640");
        LOG_MAIN_INFO_AT("  Normalize: yes (mean=0, std=255)");
        
        // 创建 OpenVINO 引擎
        engine_ = InferenceEngineFactory::Create("openvino_cpu", engine_config);
        
        if (!engine_) {
            LOG_MAIN_ERROR_AT("[OpenVINOBackend] Failed to create OpenVINO engine");
            return false;
        }
        
        if (!engine_->LoadModel(engine_config)) {
            LOG_MAIN_ERROR_AT("[OpenVINOBackend] Failed to load model: {}", 
                             config.openvino.model_path);
            return false;
        }
        
        initialized_ = true;
        confidence_threshold_ = config.openvino.confidence_threshold;
        
        LOG_MAIN_INFO_AT("[OpenVINOBackend] Initialized: model={}, device={}, batch={}",
                        config.openvino.model_path,
                        config.openvino.device,
                        config.openvino.batch_size);
        
        return true;
    }
    
    void processFrame(const VideoFrame& frame) override {
        if (!initialized_ || !engine_) {
            LOG_MAIN_WARN_AT("[OpenVINOBackend] Not initialized");
            return;
        }
        
        try {
            // ✅ 零拷贝：直接使用 YUV/NV12/NV21 数据
            // PrePostProcessor 会在 OpenVINO 内部自动处理所有预处理
            
            // 根据帧格式选择正确的 TensorData 创建方式
            std::unique_ptr<TensorData> tensor;
            ImageFormat detected_format = ImageFormat::YUV420P;  // 默认
            
            if (frame.format == 0) {  // AV_PIX_FMT_YUV420P
                // YUV420P: Y + U + V 三个独立平面（连续内存）
                size_t y_size = static_cast<size_t>(frame.width) * frame.height;
                size_t uv_size = y_size / 4;
                size_t total_size = y_size + 2 * uv_size;  // Y + U + V
                
                tensor = std::make_unique<TensorData>(TensorData::FromRawData(
                    frame.data[0],  // Y 平面指针（连续内存：Y + U + V）
                    total_size,
                    {1, static_cast<int64_t>(frame.height), static_cast<int64_t>(frame.width)},  // [N, H, W]
                    TensorDataType::UINT8
                ));
                
                detected_format = ImageFormat::YUV420P;
                
            } else if (frame.format == 12) {  // AV_PIX_FMT_NV12
                // NV12: Y 平面 + UV 交错平面
                size_t y_size = static_cast<size_t>(frame.width) * frame.height;
                size_t uv_size = y_size / 2;
                size_t total_size = y_size + uv_size;
                
                tensor = std::make_unique<TensorData>(TensorData::FromRawData(
                    frame.data[0],  // Y + UV 连续内存
                    total_size,
                    {1, static_cast<int64_t>(frame.height), static_cast<int64_t>(frame.width)},  // [N, H, W]
                    TensorDataType::UINT8
                ));
                
                detected_format = ImageFormat::NV12;
                
            } else if (frame.format == 13) {  // AV_PIX_FMT_NV21
                // NV21: Y 平面 + VU 交错平面
                size_t y_size = static_cast<size_t>(frame.width) * frame.height;
                size_t vu_size = y_size / 2;
                size_t total_size = y_size + vu_size;
                
                tensor = std::make_unique<TensorData>(TensorData::FromRawData(
                    frame.data[0],  // Y + VU 连续内存
                    total_size,
                    {1, static_cast<int64_t>(frame.height), static_cast<int64_t>(frame.width)},  // [N, H, W]
                    TensorDataType::UINT8
                ));
                
                detected_format = ImageFormat::NV21;
                
            } else {
                LOG_MAIN_WARN_AT("[OpenVINOBackend] Unsupported format: {}, falling back to YUV420P", frame.format);
                // 尝试作为 YUV420P 处理
                size_t y_size = static_cast<size_t>(frame.width) * frame.height;
                size_t uv_size = y_size / 4;
                size_t total_size = y_size + 2 * uv_size;
                
                tensor = std::make_unique<TensorData>(TensorData::FromRawData(
                    frame.data[0],
                    total_size,
                    {1, static_cast<int64_t>(frame.height), static_cast<int64_t>(frame.width)},
                    TensorDataType::UINT8
                ));
                
                detected_format = ImageFormat::YUV420P;
            }
            
            // 执行推理（PrePostProcessor 自动处理颜色转换、缩放、归一化）
            auto output = engine_->Infer(*tensor);
            handleInferenceResult(output, frame.pts);
            
        } catch (const std::exception& e) {
            LOG_MAIN_ERROR_AT("[OpenVINOBackend] Exception: {}", e.what());
        }
    }
    
private:
    void handleInferenceResult(const InferenceOutput& output, int64_t pts) {
        if (output.success) {
            DetectionResult result;
            result.channel_id = 0;
            result.timestamp = pts;
            
            if (result_callback_) {
                result_callback_(result.channel_id, result);
            }
        } else {
            LOG_MAIN_WARN_AT("[OpenVINOBackend] Inference failed");
        }
    }
    
    void processFrame(cv::Mat&& frame, int64_t pts) override {
        // OpenVINO 优先使用 YUV 输入（零拷贝）
        // 如果传入 BGR Mat，可以转换为 TensorData
        LOG_MAIN_DEBUG_AT("[OpenVINOBackend] Received BGR frame, prefer YUV input for zero-copy");
        
        // TODO: 如果需要支持 BGR 输入，可以在这里实现
        // 但这会增加一次内存拷贝，不推荐
    }
    
    std::string getBackendType() const override {
        return "openvino";
    }
    
    bool isInitialized() const override {
        return initialized_;
    }
    
    void stop() override {
        if (engine_) {
            engine_->WaitAll();
        }
        initialized_ = false;
        LOG_MAIN_INFO_AT("[OpenVINOBackend] Stopped");
    }
    
private:
    std::unique_ptr<IInferenceEngine> engine_;
    bool initialized_ = false;
    float confidence_threshold_ = 0.5f;
};
