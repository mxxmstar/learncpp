#include <iostream>
#include "alg/grpc/i_algorithm_processor.h"
#include "alg/grpc/grpc_to_alg.h"
#include "common/log/logmanager.h"

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "GrpcToAlg Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 初始化日志
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 配置处理器
        ProcessorConfig config;
        config.type = ProcessorType::GRPC_PYTHON;
        config.grpc_address = "localhost:50052";
        config.grpc_target_fps = 10;
        
        std::cout << "Creating GrpcToAlg processor..." << std::endl;
        std::cout << "  Address: " << config.grpc_address << std::endl;
        std::cout << "  Target FPS: " << config.grpc_target_fps << std::endl;
        
        // 创建处理器
        auto processor = std::make_unique<GrpcToAlg>(config);
        
        // 设置检测回调
        processor->SetDetectionCallback(
            [](const DetectionResult& result) {
                std::cout << "[Detection] Frame: " << result.frame_id 
                          << ", Boxes: " << result.boxes.size()
                          << ", Time: " << result.processing_time_ms << "ms" << std::endl;
                
                for (const auto& box : result.boxes) {
                    std::cout << "  - " << box.class_name 
                              << " (" << box.confidence << "): "
                              << "[" << box.x << ", " << box.y 
                              << ", " << box.width << "x" << box.height << "]" << std::endl;
                }
            }
        );
        
        // 启动处理器
        std::cout << "\nStarting processor..." << std::endl;
        bool success = processor->Start();
        
        if (!success) {
            std::cerr << "Failed to start processor (gRPC server may not be running)" << std::endl;
            std::cout << "\nNote: This test requires a running Python gRPC server." << std::endl;
            return 1;
        }
        
        std::cout << "Processor started successfully!" << std::endl;
        
        // 模拟处理几帧
        std::cout << "\nSimulating frame processing..." << std::endl;
        for (int i = 0; i < 5; ++i) {
            VideoFrame frame;
            frame.width = 640;
            frame.height = 480;
            frame.frame_id = "frame_" + std::to_string(i);
            frame.timestamp = i * 100;  // 每帧 100ms
            
            // 填充一些测试数据（JPEG 格式）
            frame.data.resize(1024, static_cast<uint8_t>(i % 256));
            
            bool sent = processor->ProcessFrame(frame);
            std::cout << "  Frame " << i << ": " << (sent ? "Sent" : "Failed") << std::endl;
            
            // 等待一下让异步处理完成
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        // 获取统计信息
        auto stats = processor->GetStats();
        std::cout << "\nStatistics:" << std::endl;
        std::cout << "  Frames processed: " << stats.frames_processed << std::endl;
        std::cout << "  Frames failed: " << stats.frames_failed << std::endl;
        std::cout << "  Avg processing time: " << stats.avg_processing_time_ms << "ms" << std::endl;
        std::cout << "  FPS: " << stats.fps << std::endl;
        
        // 停止处理器
        std::cout << "\nStopping processor..." << std::endl;
        processor->Stop();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test completed!" << std::endl;
        std::cout << "========================================" << std::endl;
        
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
