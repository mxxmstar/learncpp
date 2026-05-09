/**
 * PrePostProcessor 测试
 * 测试多种图像格式的预处理功能
 */

#include "alg/inference/prepost_processor.h"
#include "alg/inference/i_inference_engine.h"
#include "alg/inference/inference_engine_factory.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <random>

// 生成随机测试数据
std::vector<uint8_t> GenerateRandomData(size_t size) {
    std::vector<uint8_t> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < size; ++i) {
        data[i] = static_cast<uint8_t>(dis(gen));
    }
    
    return data;
}

void TestPrePostProcessor() {
    std::cout << "=== PrePostProcessor Test ===" << std::endl;
    
    // 1. 配置推理参数
    InferenceConfig config;
    config.model_path = "yolov5s.xml";
    config.device = "CPU";
    config.batch_size = 1;
    config.async_mode = true;
    
    // 启用 PrePostProcessor
    config.enable_preprocessor = true;
    config.preprocess_config.input_format = ImageFormat::YUV420P;
    config.preprocess_config.target_size = {640, 640};
    config.preprocess_config.normalize = true;
    config.preprocess_config.mean = {0.0f, 0.0f, 0.0f};
    config.preprocess_config.std = {255.0f, 255.0f, 255.0f};
    config.preprocess_config.output_layout = "NCHW";
    config.preprocess_config.output_type = "f32";
    
    // 2. 创建推理引擎
    auto engine = InferenceEngineFactory::Create("openvino_cpu", config);
    if (!engine) {
        std::cerr << "Failed to create inference engine" << std::endl;
        return;
    }
    
    std::cout << "Inference engine created with PrePostProcessor enabled" << std::endl;
    
    std::cout << "\n1. Loading model with PrePostProcessor..." << std::endl;
    if (!engine->LoadModel(config)) {
        std::cerr << "Failed to load model" << std::endl;
        return;
    }
    std::cout << "   ✓ Model loaded successfully" << std::endl;
    
    // 3. 检查模型输入信息
    auto input_info = engine->GetInputInfo();
    if (!input_info.empty()) {
        std::cout << "\n2. Model input info:" << std::endl;
        std::cout << "   Shape: [";
        for (size_t i = 0; i < input_info[0].shape.size(); ++i) {
            std::cout << input_info[0].shape[i];
            if (i < input_info[0].shape.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
        std::cout << "   Dtype: " << input_info[0].dtype << std::endl;
    }
    
    // 4. 测试不同格式的输入
    std::cout << "\n3. Testing different input formats..." << std::endl;
    
    int width = 1920;
    int height = 1080;
    
    // 测试 NV12
    {
        std::cout << "   Testing NV12 format..." << std::endl;
        size_t nv12_size = width * height * 3 / 2;
        auto nv12_data = GenerateRandomData(nv12_size);
        
        TensorData tensor = TensorData::FromRawData(
            nv12_data.data(),
            nv12_size,
            {1, height, width},
            TensorDataType::UINT8
        );
        
        auto start = std::chrono::high_resolution_clock::now();
        auto output = engine->Infer(tensor);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        if (output.success) {
            std::cout << "      ✓ NV12 inference successful (" << duration << " ms)" << std::endl;
        } else {
            std::cerr << "      ✗ NV12 inference failed: " << output.error_message << std::endl;
        }
    }
    
    // 测试 RGB
    {
        std::cout << "   Testing RGB format..." << std::endl;
        
        // 修改配置为 RGB
        config.preprocess_config.input_format = ImageFormat::RGB;
        engine->LoadModel(config);  // 重新加载
        
        size_t rgb_size = width * height * 3;
        auto rgb_data = GenerateRandomData(rgb_size);
        
        TensorData tensor = TensorData::FromRawData(
            rgb_data.data(),
            rgb_size,
            {1, height, width, 3},  // NHWC layout
            TensorDataType::UINT8
        );
        
        auto start = std::chrono::high_resolution_clock::now();
        auto output = engine->Infer(tensor);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        if (output.success) {
            std::cout << "      ✓ RGB inference successful (" << duration << " ms)" << std::endl;
        } else {
            std::cerr << "      ✗ RGB inference failed: " << output.error_message << std::endl;
        }
    }
    
    std::cout << "\n=== Test Completed ===" << std::endl;
}

int main() {
    try {
        TestPrePostProcessor();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
