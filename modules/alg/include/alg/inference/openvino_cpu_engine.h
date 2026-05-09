#pragma once

#include "alg/inference/i_inference_engine.h"
#include "alg/inference/tensor_data.h"
#include "alg/inference/prepost_processor.h"
#include <openvino/openvino.hpp>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <chrono>
#include <memory>

/// @brief OpenVINO CPU 推理引擎
class OpenVinoCpuEngine : public IInferenceEngine {
public:
    OpenVinoCpuEngine();
    ~OpenVinoCpuEngine() override;
    
    /// @brief 加载模型
    bool LoadModel(const InferenceConfig& config) override;
    
    /// @brief 同步推理
    InferenceOutput Infer(const TensorData& input) override;
    
    /// @brief 异步推理
    bool InferAsync(const TensorData& input, 
                   InferenceCallback callback) override;
    
    /// @brief 批量推理
    std::vector<InferenceOutput> InferBatch(
        const std::vector<TensorData>& inputs) override;
    
    /// @brief 等待所有异步推理完成
    bool WaitAll() override;
    
    /// @brief 获取输入/输出信息
    std::vector<TensorInfo> GetInputInfo() const override;
    std::vector<TensorInfo> GetOutputInfo() const override;
    
    /// @brief 获取引擎类型
    InferenceEngineType GetType() const override {
        return InferenceEngineType::OPENVINO_CPU;
    }
    
    /// @brief 检查引擎是否可用
    bool IsAvailable() const override {
        return initialized_;
    }
    
    /// @brief 获取统计信息
    Stats GetStats() const override;

private:
    /// @brief 工作线程函数
    void WorkerLoop();
    
    /// @brief 执行单次推理
    InferenceOutput ExecuteInference(const TensorData& input);
    
    // ==================== 成员变量 ====================
    /// @brief OpenVINO Core
    ov::Core core_;
    
    /// @brief 编译后的模型
    ov::CompiledModel compiled_model_;
    
    /// @brief 推理请求池（支持并发）
    std::vector<ov::InferRequest> infer_requests_;
    
    /// @brief 当前请求索引
    std::atomic<int> current_request_idx_{0};
    
    /// @brief 配置
    InferenceConfig config_;
    
    /// @brief 初始化状态
    bool initialized_ = false;
    
    /// @brief 异步模式
    bool async_mode_ = false;
    
    /// @brief 工作线程
    std::thread worker_thread_;
    
    /// @brief 运行标志
    std::atomic<bool> running_{false};
    
    /// @brief 任务队列
    struct AsyncTask {
        TensorData input;
        InferenceCallback callback;
    };
    std::queue<AsyncTask> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    /// @brief 统计信息
    mutable std::mutex stats_mutex_;
    uint64_t total_inferences_ = 0;
    uint64_t total_errors_ = 0;
    double total_time_ms_ = 0.0;
    std::chrono::steady_clock::time_point start_time_;
    
    /// @brief 辅助方法：将 uint8 转换为 float 并归一化到 [0, 1]
    /// @param src 源数据（uint8）
    /// @param dst 目标数据（float）
    /// @param count 元素数量
    void ConvertUint8ToFloat(const uint8_t* src, float* dst, size_t count);
    
    // ==================== PrePostProcessor 支持 ====================
    /// @brief 预处理器（可选）
    std::unique_ptr<PrePostProcessor> preprocessor_;
    
    /// @brief 是否启用预处理
    bool use_preprocessor_ = false;
};
