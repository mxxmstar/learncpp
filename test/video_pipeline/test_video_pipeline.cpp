#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "video_pipeline/video_pipeline.h"
#include "common/log/logmanager.h"

// 全局标志
std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    g_running = false;
}

int main() {
    // 设置信号处理
    std::signal(SIGINT, signalHandler);
    
    std::cout << "========================================" << std::endl;
    std::cout << "VideoPipeline Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 初始化日志
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 创建 io_context
        boost::asio::io_context io_ctx;
        
        // 配置流水线
        PipelineConfig config;
        config.channel_id = 1;
        config.stream_url = "http://127.0.0.1/live/proxy_cam1.live.flv";
        config.reconnect_delay = 3;
        config.max_reconnect_attempts = -1;  // 无限重试
        config.decoder_threads = 2;
        config.raw_queue_size = 64;
        config.decoded_queue_size = 16;
        config.processed_queue_size = 16;
        
        // 添加滤镜链
        config.filters = {
            "hist_eq",        // 直方图均衡化
            "gaussian_blur",  // 高斯模糊
            "grayscale"       // 灰度化
        };
        config.enable_preprocess = true;
        config.target_width = 640;
        config.target_height = 480;
        
        std::cout << "Pipeline Configuration:" << std::endl;
        std::cout << "  Channel ID: " << config.channel_id << std::endl;
        std::cout << "  Stream URL: " << config.stream_url << std::endl;
        std::cout << "  Filters: ";
        for (const auto& f : config.filters) {
            std::cout << f << " ";
        }
        std::cout << std::endl;
        std::cout << "  Target Size: " << config.target_width << "x" 
                  << config.target_height << std::endl;
        std::cout << "\nPress Ctrl+C to stop...\n" << std::endl;
        
        // 创建流水线
        VideoPipeline pipeline(io_ctx, config);
        
        // 设置输出回调（接收处理后的帧）
        int output_count = 0;
        pipeline.setFrameOutputCallback(
            [&output_count](int channel_id, cv::Mat&& frame, int64_t pts) {
                output_count++;
                
                if (output_count % 30 == 0) {
                    std::cout << "[Output] Channel=" << channel_id
                              << ", Frame=" << output_count
                              << ", Size=" << frame.cols << "x" << frame.rows
                              << ", PTS=" << pts << "ms" << std::endl;
                }
                
                // 这里可以将帧传递给算法模块
                // 或者保存到队列中
            }
        );
        
        // 启动流水线
        bool success = pipeline.start();
        if (!success) {
            std::cerr << "Failed to start pipeline" << std::endl;
            return 1;
        }
        
        std::cout << "Pipeline started successfully!" << std::endl;
        
        // 在后台线程中运行 io_context（处理异步网络操作）
        std::thread io_thread([&io_ctx]() {
            std::cout << "[IO Thread] Running io_context..." << std::endl;
            io_ctx.run();
            std::cout << "[IO Thread] io_context stopped." << std::endl;
        });
        
        // 主循环等待
        while (g_running && pipeline.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 打印统计信息
            uint64_t received = pipeline.getFramesReceived();
            uint64_t decoded = pipeline.getFramesDecoded();
            uint64_t processed = pipeline.getFramesProcessed();
            
            std::cout << "[Stats] Received=" << received
                      << ", Decoded=" << decoded
                      << ", Processed=" << processed
                      << std::endl;
        }
        
        // 停止流水线
        std::cout << "\nStopping pipeline..." << std::endl;
        pipeline.stop();
        
        // 停止 io_context
        io_ctx.stop();
        
        // 等待 io 线程结束
        if (io_thread.joinable()) {
            io_thread.join();
        }
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test completed!" << std::endl;
        std::cout << "Total frames - Received: " << pipeline.getFramesReceived()
                  << ", Decoded: " << pipeline.getFramesDecoded()
                  << ", Processed: " << pipeline.getFramesProcessed() << std::endl;
        std::cout << "========================================" << std::endl;
        
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
