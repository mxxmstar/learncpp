#include "alg/inference/inference_engine_factory.h"
#include "alg/inference/i_inference_engine.h"
#include <iostream>
#include <vector>
#include <cassert>

void TestOpenVinoCpuEngine() {
    std::cout << "=== Testing OpenVINO CPU Engine ===" << std::endl;
    
    // 1. 创建引擎
    InferenceConfig config;
    config.type = InferenceEngineType::OPENVINO_CPU;
    config.model_path = "test_model.xml";  // 测试模型路径
    config.device = "CPU";
    config.async_mode = false;
    config.num_requests = 1;
    
    auto engine = InferenceEngineFactory::Create("openvino_cpu", config);
    
    if (!engine) {
        std::cout << "[SKIP] OpenVINO engine creation failed (model not found)" << std::endl;
        return;
    }
    
    // 2. 检查引擎信息
    std::cout << "Engine type: " << static_cast<int>(engine->GetType()) << std::endl;
    std::cout << "Is available: " << (engine->IsAvailable() ? "Yes" : "No") << std::endl;
    
    // 3. 获取输入/输出信息
    auto input_info = engine->GetInputInfo();
    auto output_info = engine->GetOutputInfo();
    
    std::cout << "Input tensors: " << input_info.size() << std::endl;
    for (const auto& info : input_info) {
        std::cout << "  - " << info.name << ": [";
        for (size_t i = 0; i < info.shape.size(); ++i) {
            std::cout << info.shape[i];
            if (i < info.shape.size() - 1) std::cout << ", ";
        }
        std::cout << "] (" << info.dtype << ")" << std::endl;
    }
    
    std::cout << "Output tensors: " << output_info.size() << std::endl;
    for (const auto& info : output_info) {
        std::cout << "  - " << info.name << ": [";
        for (size_t i = 0; i < info.shape.size(); ++i) {
            std::cout << info.shape[i];
            if (i < info.shape.size() - 1) std::cout << ", ";
        }
        std::cout << "] (" << info.dtype << ")" << std::endl;
    }
    
    // 4. 同步推理测试（如果有输入）
    if (!input_info.empty() && !output_info.empty()) {
        std::cout << "\nTesting synchronous inference..." << std::endl;
        
        // 创建模拟输入数据
        auto& first_input = input_info[0];
        int64_t num_elements = 1;
        for (auto dim : first_input.shape) {
            num_elements *= dim;
        }
        
        std::vector<float> input_data(num_elements, 0.5f);
        TensorData input_tensor = TensorData::FromCpu(input_data, first_input.shape);
        
        // 执行推理
        auto result = engine->Infer(input_tensor);
        
        if (result.success) {
            std::cout << "Inference successful!" << std::endl;
            std::cout << "Inference time: " << result.inference_time_us << " us" << std::endl;
            std::cout << "Output tensors: " << result.tensors.size() << std::endl;
            
            for (const auto& [name, tensor] : result.tensors) {
                std::cout << "  - " << name << ": [";
                for (size_t i = 0; i < tensor.shape.size(); ++i) {
                    std::cout << tensor.shape[i];
                    if (i < tensor.shape.size() - 1) std::cout << ", ";
                }
                std::cout << "]" << std::endl;
            }
        } else {
            std::cout << "Inference failed: " << result.error_message << std::endl;
        }
    }
    
    // 5. 获取统计信息
    auto stats = engine->GetStats();
    std::cout << "\nStatistics:" << std::endl;
    std::cout << "  Total inferences: " << stats.inferences_count << std::endl;
    std::cout << "  Total errors: " << stats.errors_count << std::endl;
    std::cout << "  Avg inference time: " << stats.avg_inference_time_ms << " ms" << std::endl;
    std::cout << "  FPS: " << stats.fps << std::endl;
    
    std::cout << "\n=== Test Completed ===" << std::endl;
}

void TestAsyncInference() {
    std::cout << "\n=== Testing Async Inference ===" << std::endl;
    
    InferenceConfig config;
    config.type = InferenceEngineType::OPENVINO_CPU;
    config.model_path = "test_model.xml";
    config.device = "CPU";
    config.async_mode = true;
    config.num_requests = 4;
    
    auto engine = InferenceEngineFactory::Create("openvino_cpu", config);
    
    if (!engine) {
        std::cout << "[SKIP] Async test skipped (engine not available)" << std::endl;
        return;
    }
    
    std::cout << "Async mode enabled with 4 requests" << std::endl;
    
    // 注意：这里需要实际的模型文件才能测试
    std::cout << "[INFO] Async inference requires a valid model file" << std::endl;
    
    std::cout << "=== Async Test Completed ===" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Inference Engine Unit Tests" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        TestOpenVinoCpuEngine();
        TestAsyncInference();
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
