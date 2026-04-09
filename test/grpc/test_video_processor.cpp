/**
 * 算法处理器接口测试
 * 测试 IAlgorithmProcessor 抽象层和 GrpcAlgorithmProcessor 实现
 */

#include "video_pipeline/algorithm_processor/i_algorithm_processor.h"
#include "video_pipeline/algorithm_processor/grpc_algorithm_processor.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace video_pipeline::algorithm_processor;

void TestGrpcAlgorithmProcessor() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Test: GrpcAlgorithmProcessor" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // 创建配置
    ProcessorConfig config;
    config.type = ProcessorType::GRPC_PYTHON;
    config.grpc_address = "localhost:50052";
    config.grpc_target_fps = 10;
    
    // 创建处理器
    auto processor = std::make_unique<GrpcAlgorithmProcessor>(config);
    
    // 设置回调
    processor->SetDetectionCallback([](const DetectionResult& result) {
        std::cout << "[Callback] Frame: " << result.frame_id 
                 << ", Boxes: " << result.boxes.size()
                 << ", Time: " << result.processing_time_ms << "ms"
                 << ", Algorithm: " << result.algorithm << std::endl;
        
        for (size_t i = 0; i < result.boxes.size() && i < 3; ++i) {
            const auto& box = result.boxes[i];
            std::cout << "  Box " << i << ": (" 
                     << box.x << ", " << box.y << ") "
                     << box.width << "x" << box.height
                     << " conf=" << box.confidence << std::endl;
        }
    });
    
    // 启动处理器
    std::cout << "\nStarting processor..." << std::endl;
    if (!processor->Start()) {
        std::cerr << "Failed to start processor" << std::endl;
        return;
    }
    
    std::cout << "Processor started. Type: " 
              << (processor->GetType() == ProcessorType::GRPC_PYTHON ? "gRPC" : "Native")
              << std::endl;
    
    // 模拟发送帧
    std::cout << "\nSending test frames..." << std::endl;
    
    for (int i = 0; i < 20; ++i) {
        VideoFrame frame;
        frame.frame_id = "test_frame_" + std::to_string(i);
        frame.width = 640;
        frame.height = 480;
        frame.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // 创建模拟的 JPEG 数据（实际使用时应该是真实的 JPEG）
        frame.data.resize(1024, static_cast<uint8_t>(i % 256));
        
        if (processor->ProcessFrame(frame)) {
            std::cout << "Frame " << i << " sent" << std::endl;
        } else {
            std::cerr << "Frame " << i << " failed" << std::endl;
        }
        
        // 控制帧率
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 获取统计信息
    auto stats = processor->GetStats();
    std::cout << "\n--- Statistics ---" << std::endl;
    std::cout << "Frames processed: " << stats.frames_processed << std::endl;
    std::cout << "Frames failed: " << stats.frames_failed << std::endl;
    std::cout << "Avg processing time: " << stats.avg_processing_time_ms << "ms" << std::endl;
    std::cout << "FPS: " << stats.fps << std::endl;
    
    // 停止处理器
    std::cout << "\nStopping processor..." << std::endl;
    processor->Stop();
    
    std::cout << "Test completed!" << std::endl;
}

int main() {
    std::cout << "\n" << std::string(60, '#') << std::endl;
    std::cout << "# Algorithm Processor Interface Test" << std::endl;
    std::cout << "# Testing IAlgorithmProcessor abstraction layer" << std::endl;
    std::cout << std::string(60, '#') << std::endl;
    
    try {
        TestGrpcAlgorithmProcessor();
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
