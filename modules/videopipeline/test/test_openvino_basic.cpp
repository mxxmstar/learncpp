/**
 * OpenVINO 基础功能测试
 * 用于诊断 OpenVINO 环境配置问题
 */

#include <iostream>
#include <string>
#include <openvino/openvino.hpp>

int main() {
    std::cout << "=== OpenVINO Basic Test ===" << std::endl;
    
    try {
        // 1. 创建 Core 对象
        std::cout << "\n1. Creating ov::Core..." << std::endl;
        ov::Core core;
        std::cout << "   ✓ Core created successfully" << std::endl;
        
        // 2. 检查可用设备
        std::cout << "\n2. Checking available devices..." << std::endl;
        auto devices = core.get_available_devices();
        std::cout << "   Available devices: ";
        if (devices.empty()) {
            std::cout << "(NONE)" << std::endl;
            std::cout << "   ✗ ERROR: No devices found!" << std::endl;
        } else {
            for (const auto& device : devices) {
                std::cout << device << " ";
            }
            std::cout << std::endl;
            std::cout << "   ✓ Found " << devices.size() << " device(s)" << std::endl;
        }
        
        // 3. 检查已注册的前端
        std::cout << "\n3. Checking registered frontends..." << std::endl;
        // OpenVINO 2023+ 不再直接提供 frontend 列表，通过加载模型测试
        
        // 4. 尝试读取模型（如果存在）
        std::string model_path = "yolov5s.xml";
        std::cout << "\n4. Attempting to read model: " << model_path << std::endl;
        
        #include <filesystem>
        if (!std::filesystem::exists(model_path)) {
            std::cout << "   ⚠ Model file not found, skipping model load test" << std::endl;
        } else {
            try {
                auto model = core.read_model(model_path);
                std::cout << "   ✓ Model loaded successfully!" << std::endl;
                std::cout << "   Model name: " << model->get_friendly_name() << std::endl;
            } catch (const std::exception& e) {
                std::cout << "   ✗ Failed to load model: " << e.what() << std::endl;
            }
        }
        
        std::cout << "\n=== Test Completed ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Exception: " << e.what() << std::endl;
        return 1;
    }
}