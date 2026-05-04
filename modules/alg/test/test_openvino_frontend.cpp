/**
 * @file test_openvino_frontend.cpp
 * @brief 测试 OpenVINO frontend 加载
 */

#include <openvino/openvino.hpp>
#include <iostream>
#include <filesystem>

#ifdef _WIN32
#include <cstdlib>  // for _putenv_s
#endif

int main() {
    std::cout << "=== OpenVINO Frontend Test ===" << std::endl;
    std::cout << std::endl;
    
    // 打印当前目录
    std::cout << "Current directory: " << std::filesystem::current_path() << std::endl;
    std::cout << std::endl;
    
    // 检查 DLL 文件
    std::cout << "Checking DLL files:" << std::endl;
    std::cout << "  openvino.dll exists: " 
              << std::filesystem::exists("openvino.dll") << std::endl;
    std::cout << "  openvino_ir_frontend.dll exists: " 
              << std::filesystem::exists("openvino_ir_frontend.dll") << std::endl;
    std::cout << std::endl;
    
    try {
        // 创建 Core 对象
        std::cout << "Creating ov::Core..." << std::endl;
        
        // 尝试使用不同的方法初始化
        #ifdef _WIN32
        // 方法1: 添加当前目录到 PATH
        auto current_dir = std::filesystem::current_path().string();
        std::string path_env;
        const char* existing_path = getenv("PATH");
        if (existing_path) {
            path_env = existing_path;
        }
        path_env = current_dir + ";" + path_env;
        _putenv_s("PATH", path_env.c_str());
        std::cout << "Added current directory to PATH" << std::endl;
        #endif
        
        ov::Core core;
        std::cout << "Core created successfully!" << std::endl;
        std::cout << std::endl;
        
        // 尝试列出设备
        std::cout << "Available devices:" << std::endl;
        auto devices = core.get_available_devices();
        for (const auto& device : devices) {
            std::cout << "  - " << device << std::endl;
        }
        std::cout << std::endl;
        
        // 尝试加载模型
        std::string model_path = "yolov5s.xml";
        if (std::filesystem::exists(model_path)) {
            std::cout << "Model file found: " << model_path << std::endl;
            std::cout << "Attempting to load model..." << std::endl;
            
            try {
                auto model = core.read_model(model_path);
                std::cout << "SUCCESS: Model loaded!" << std::endl;
                std::cout << "Model inputs: " << model->inputs().size() << std::endl;
                std::cout << "Model outputs: " << model->outputs().size() << std::endl;
                return 0;
            } catch (const std::exception& e) {
                std::cerr << "ERROR loading model: " << e.what() << std::endl;
                std::cerr << std::endl;
                std::cerr << "This suggests the IR frontend is not loaded." << std::endl;
                std::cerr << "Please ensure openvino_ir_frontend.dll is in PATH or current directory." << std::endl;
                return 1;
            }
        } else {
            std::cout << "Model file not found: " << model_path << std::endl;
            std::cout << "Skipping model load test." << std::endl;
            return 0;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
