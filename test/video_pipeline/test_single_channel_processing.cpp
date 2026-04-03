#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "video_pipeline/video_pipeline.h"
#include "video_pipeline/algorithm/base_algorithm.h"
#include "video_pipeline/output/result_output.h"
#include "log/logmanager.h"

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
    std::cout << "Single Channel Video Processing Test" << std::endl;
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
        config.stream_url = "http://127.0.0.1:8080/live/test.flv";
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
        
        // 创建算法处理器
        std::unique_ptr<IAlgorithm> algorithm;
        
        // 选择算法类型
        std::cout << "Select algorithm type:" << std::endl;
        std::cout << "  1. Null Algorithm (test only)" << std::endl;
        std::cout << "  2. Motion Detection" << std::endl;
        std::cout << "Default: Motion Detection" << std::endl;
        
        int algo_choice = 2;
        if (algo_choice == 1) {
            algorithm = std::make_unique<NullAlgorithm>();
        } else {
            algorithm = std::make_unique<MotionDetectionAlgorithm>();
        }
        
        std::cout << "Using algorithm: " << algorithm->getName() << std::endl;
        
        // 创建结果输出器（组合使用）
        std::vector<std::shared_ptr<IResultOutput>> outputs;
        outputs.push_back(std::make_shared<ConsoleOutput>());  // 控制台输出
        outputs.push_back(std::make_shared<LogOutput>());      // 日志输出
        
        // 可选：文件输出
        // outputs.push_back(std::make_shared<FileOutput>("results.jsonl"));
        
        // 设置帧输出回调（在 VideoPipeline 中处理算法）
        int processed_count = 0;
        pipeline.setFrameOutputCallback(
            [&processed_count, &algorithm, &outputs](
                int channel_id, cv::Mat&& frame, int64_t pts) {
                
                processed_count++;
                
                // 运行算法
                AlgorithmResult result = algorithm->process(frame, channel_id, pts);
                
                // 输出到所有输出器
                for (auto& output : outputs) {
                    output->output(result);
                }
                
                // 每 100 帧打印统计
                if (processed_count % 100 == 0) {
                    std::cout << "[Stats] Processed " << processed_count 
                              << " frames" << std::endl;
                }
            }
        );
        
        // 启动流水线
        bool success = pipeline.start();
        if (!success) {
            std::cerr << "Failed to start pipeline" << std::endl;
            return 1;
        }
        
        std::cout << "Pipeline started successfully!" << std::endl;
        
        // 主循环等待
        while (g_running && pipeline.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 打印统计信息
            uint64_t received = pipeline.getFramesReceived();
            uint64_t decoded = pipeline.getFramesDecoded();
            uint64_t processed = pipeline.getFramesProcessed();
            
            std::cout << "[Pipeline Stats] Received=" << received
                      << ", Decoded=" << decoded
                      << ", Processed=" << processed
                      << ", Algorithm=" << processed_count
                      << std::endl;
        }
        
        // 停止流水线
        std::cout << "\nStopping pipeline..." << std::endl;
        pipeline.stop();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test completed!" << std::endl;
        std::cout << "Total frames - Received: " << pipeline.getFramesReceived()
                  << ", Decoded: " << pipeline.getFramesDecoded()
                  << ", Processed: " << pipeline.getFramesProcessed()
                  << ", Algorithm: " << processed_count << std::endl;
        std::cout << "========================================" << std::endl;
        
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
