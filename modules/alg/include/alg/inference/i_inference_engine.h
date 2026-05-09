#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <memory>
#include <map>
#include <string>
#include "alg/inference/prepost_processor.h"  // 包含 PreProcessConfig

// 前向声明
struct TensorData;

/// @brief 推理引擎类型
enum class InferenceEngineType {
    OPENVINO_CPU,      ///< OpenVINO CPU
    OPENVINO_GPU,      ///< OpenVINO GPU (Intel)
    TENSORRT,          ///< NVIDIA TensorRT
    ONNXRUNTIME_CPU,   ///< ONNX Runtime CPU
    ONNXRUNTIME_CUDA,  ///< ONNX Runtime CUDA
    COREML             ///< Apple CoreML
};

/// @brief 推理配置
struct InferenceConfig {
    InferenceEngineType type = InferenceEngineType::OPENVINO_CPU;
    std::string model_path;
    std::string device = "CPU";  ///< CPU, GPU, MULTI:CPU,GPU
    bool async_mode = true;
    int num_requests = 4;
    int batch_size = 1;
    int gpu_device_id = 0;
    
    // TensorRT 特定
    int max_workspace_size_mb = 512;
    bool fp16_mode = false;
    bool int8_mode = false;
    
    // PrePostProcessor 配置（可选）
    bool enable_preprocessor = false;
    PreProcessConfig preprocess_config;
};

/// @brief 推理结果
struct InferenceOutput {
    std::map<std::string, TensorData> tensors; ///< 输出张量
    int64_t inference_time_us = 0;             ///< 推理耗时（微秒）
    bool success = false;
    std::string error_message;
};

/// @brief 推理完成回调
using InferenceCallback = std::function<void(const InferenceOutput& output)>;

/// @brief 推理引擎接口
class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;
    
    /// @brief 加载模型
    virtual bool LoadModel(const InferenceConfig& config) = 0;
    
    /// @brief 同步推理
    virtual InferenceOutput Infer(const TensorData& input) = 0;
    
    /// @brief 异步推理
    virtual bool InferAsync(const TensorData& input, 
                           InferenceCallback callback) = 0;
    
    /// @brief 批量推理
    virtual std::vector<InferenceOutput> InferBatch(
        const std::vector<TensorData>& inputs) = 0;
    
    /// @brief 等待所有异步推理完成
    virtual bool WaitAll() = 0;
    
    /// @brief 获取输入/输出信息
    struct TensorInfo {
        std::string name;
        std::vector<int64_t> shape;
        std::string dtype;  ///< FP32, INT8, etc.
    };
    virtual std::vector<TensorInfo> GetInputInfo() const = 0;
    virtual std::vector<TensorInfo> GetOutputInfo() const = 0;
    
    /// @brief 获取引擎类型
    virtual InferenceEngineType GetType() const = 0;
    
    /// @brief 检查引擎是否可用
    virtual bool IsAvailable() const = 0;
    
    /// @brief 获取统计信息
    struct Stats {
        uint64_t inferences_count = 0;
        uint64_t errors_count = 0;
        double avg_inference_time_ms = 0.0;
        double fps = 0.0;
    };
    virtual Stats GetStats() const = 0;
};
