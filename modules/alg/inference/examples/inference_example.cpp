/**
 * @file inference_example.cpp
 * @brief Inference 模块使用示例
 * 
 * 展示如何使用推理引擎进行同步/异步推理
 */

#include "alg/inference/inference_engine_factory.h"
#include "alg/inference/i_inference_engine.h"
#include "alg/inference/tensor_data.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>

/// @brief 示例 1: 基本同步推理
void Example_SyncInference() {
    std::cout << "\n=== Example 1: Synchronous Inference ===" << std::endl;
    
    // 1. 配置引擎
    InferenceConfig config;
    config.model_path = "models/yolov5s.xml";  // 替换为实际模型路径
    config.device = "CPU";
    config.async_mode = false;
    config.num_requests = 1;
    
    // 2. 创建引擎
    auto engine = InferenceEngineFactory::Create("openvino_cpu", config);
    
    if (!engine) {
        std::cerr << "Failed to create engine (model not found)" << std::endl;
        return;
    }
    
    std::cout << "Engine created successfully" << std::endl;
    
    // 3. 打印模型信息
    auto input_info = engine->GetInputInfo();
    auto output_info = engine->GetOutputInfo();
    
    std::cout << "Model info:" << std::endl;
    std::cout << "  Inputs: " << input_info.size() << std::endl;
    for (const auto& info : input_info) {
        std::cout << "    - " << info.name << ": [";
        for (size_t i = 0; i < info.shape.size(); ++i) {
            std::cout << info.shape[i];
            if (i < info.shape.size() - 1) std::cout << "x";
        }
        std::cout << "] (" << info.dtype << ")" << std::endl;
    }
    
    std::cout << "  Outputs: " << output_info.size() << std::endl;
    for (const auto& info : output_info) {
        std::cout << "    - " << info.name << ": [";
        for (size_t i = 0; i < info.shape.size(); ++i) {
            std::cout << info.shape[i];
            if (i < info.shape.size() - 1) std::cout << "x";
        }
        std::cout << "] (" << info.dtype << ")" << std::endl;
    }
    
    // 4. 准备输入数据（模拟图像预处理后的张量）
    if (!input_info.empty()) {
        const auto& first_input = input_info[0];
        
        // 计算元素总数
        int64_t num_elements = 1;
        for (auto dim : first_input.shape) {
            num_elements *= dim;
        }
        
        // 创建模拟数据（全 0.5）
        std::vector<float> input_data(num_elements, 0.5f);
        TensorData input_tensor = TensorData::FromCpu(input_data, first_input.shape);
        
        std::cout << "\nInput tensor size: " << num_elements << " elements" << std::endl;
        std::cout << "Input data size: " << input_data.size() * sizeof(float) << " bytes" << std::endl;
        
        // 5. 执行推理
        std::cout << "\nRunning inference..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        auto result = engine->Infer(input_tensor);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        // 6. 处理结果
        if (result.success) {
            std::cout << "✓ Inference successful!" << std::endl;
            std::cout << "  Total time: " << elapsed_ms << " ms" << std::endl;
            std::cout << "  Inference time: " << result.inference_time_us << " us" << std::endl;
            std::cout << "  Output tensors: " << result.tensors.size() << std::endl;
            
            for (const auto& [name, tensor] : result.tensors) {
                std::cout << "    - " << name << ": [";
                for (size_t i = 0; i < tensor.shape.size(); ++i) {
                    std::cout << tensor.shape[i];
                    if (i < tensor.shape.size() - 1) std::cout << "x";
                }
                std::cout << "]" << std::endl;
            }
        } else {
            std::cerr << "✗ Inference failed: " << result.error_message << std::endl;
        }
    }
    
    // 7. 获取统计信息
    auto stats = engine->GetStats();
    std::cout << "\nStatistics:" << std::endl;
    std::cout << "  Inferences: " << stats.inferences_count << std::endl;
    std::cout << "  Errors: " << stats.errors_count << std::endl;
    if (stats.inferences_count > 0) {
        std::cout << "  Avg time: " << stats.avg_inference_time_ms << " ms" << std::endl;
    }
}

/// @brief 示例 2: 异步推理
void Example_AsyncInference() {
    std::cout << "\n=== Example 2: Asynchronous Inference ===" << std::endl;
    
    // 1. 配置引擎（启用异步模式）
    InferenceConfig config;
    config.model_path = "models/yolov5s.xml";
    config.device = "CPU";
    config.async_mode = true;
    config.num_requests = 4;  // 4个并发请求
    
    auto engine = InferenceEngineFactory::Create("openvino_cpu", config);
    
    if (!engine) {
        std::cerr << "Failed to create engine" << std::endl;
        return;
    }
    
    std::cout << "Async engine created with 4 requests" << std::endl;
    
    // 2. 准备输入数据
    auto input_info = engine->GetInputInfo();
    if (input_info.empty()) {
        std::cerr << "No input info available" << std::endl;
        return;
    }
    
    const auto& first_input = input_info[0];
    int64_t num_elements = 1;
    for (auto dim : first_input.shape) {
        num_elements *= dim;
    }
    
    std::vector<float> input_data(num_elements, 0.5f);
    TensorData input_tensor = TensorData::FromCpu(input_data, first_input.shape);
    
    // 3. 发送多个异步推理请求
    std::cout << "\nSending 5 async inference requests..." << std::endl;
    
    int completed_count = 0;
    std::mutex count_mutex;
    
    for (int i = 0; i < 5; ++i) {
        bool success = engine->InferAsync(input_tensor, 
            [&completed_count, &count_mutex, i](const InferenceOutput& output) {
                std::lock_guard<std::mutex> lock(count_mutex);
                completed_count++;
                
                if (output.success) {
                    std::cout << "  Request " << i << " completed in " 
                              << output.inference_time_us << " us" << std::endl;
                } else {
                    std::cerr << "  Request " << i << " failed: " 
                              << output.error_message << std::endl;
                }
            });
        
        if (success) {
            std::cout << "  Request " << i << " sent" << std::endl;
        }
        
        // 稍微延迟，模拟真实场景
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 4. 等待所有请求完成
    std::cout << "\nWaiting for all requests to complete..." << std::endl;
    engine->WaitAll();
    
    std::cout << "All requests completed: " << completed_count << "/5" << std::endl;
    
    // 5. 获取统计信息
    auto stats = engine->GetStats();
    std::cout << "\nStatistics:" << std::endl;
    std::cout << "  Total inferences: " << stats.inferences_count << std::endl;
    std::cout << "  FPS: " << stats.fps << std::endl;
}

/// @brief 示例 3: 批量推理
void Example_BatchInference() {
    std::cout << "\n=== Example 3: Batch Inference ===" << std::endl;
    
    // 1. 创建引擎
    InferenceConfig config;
    config.model_path = "models/yolov5s.xml";
    config.device = "CPU";
    config.async_mode = false;
    
    auto engine = InferenceEngineFactory::Create("openvino_cpu", config);
    
    if (!engine) {
        std::cerr << "Failed to create engine" << std::endl;
        return;
    }
    
    // 2. 准备批量输入
    auto input_info = engine->GetInputInfo();
    if (input_info.empty()) {
        std::cerr << "No input info available" << std::endl;
        return;
    }
    
    const auto& first_input = input_info[0];
    int64_t num_elements = 1;
    for (auto dim : first_input.shape) {
        num_elements *= dim;
    }
    
    int batch_size = 10;
    std::vector<TensorData> inputs;
    
    std::cout << "Preparing " << batch_size << " input tensors..." << std::endl;
    for (int i = 0; i < batch_size; ++i) {
        std::vector<float> data(num_elements, static_cast<float>(i) / batch_size);
        inputs.push_back(TensorData::FromCpu(data, first_input.shape));
    }
    
    // 3. 执行批量推理
    std::cout << "Running batch inference..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    auto results = engine->InferBatch(inputs);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    // 4. 处理结果
    int success_count = 0;
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].success) {
            success_count++;
        }
    }
    
    std::cout << "Batch inference completed:" << std::endl;
    std::cout << "  Total time: " << elapsed_ms << " ms" << std::endl;
    std::cout << "  Success: " << success_count << "/" << batch_size << std::endl;
    std::cout << "  Avg time per frame: " 
              << (elapsed_ms / static_cast<double>(batch_size)) << " ms" << std::endl;
}

/// @brief 主函数
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Inference Module Examples" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        // 运行示例
        Example_SyncInference();
        Example_AsyncInference();
        Example_BatchInference();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "All examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
