#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "video_pipeline/video_pipeline_manager.h"
#include "video_pipeline/algorithm/base_algorithm.h"
#include "video_pipeline/output/result_output.h"
#include "video_pipeline/processor/osd_renderer.h"
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
    std::cout << "Multi-Channel Video Processing Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 初始化日志
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 创建 io_context
        boost::asio::io_context io_ctx;
        
        // 初始化流水线管理器
        auto& manager = VideoPipelineManager::getInstance();
        manager.initialize(io_ctx);
        
        std::cout << "VideoPipelineManager initialized" << std::endl;
        
        // 配置多个通道
        struct ChannelConfig {
            int channel_id;
            std::string stream_url;
            std::vector<std::string> filters;
            int target_width;
            int target_height;
        };
        
        std::vector<ChannelConfig> channels = {
            {1, "http://127.0.0.1/live/proxy_cam1.live.flv", 
             {"hist_eq", "gaussian_blur"}, 640, 480},
            {2, "http://127.0.0.1/live/proxy_cam2.live.flv",
             {"grayscale", "canny"}, 640, 480},
            {3, "http://127.0.0.1/live/proxy_cam3.live.flv",
             {"median_blur", "threshold"}, 640, 480},
        };
        
        // 添加所有通道
        for (const auto& ch : channels) {
            PipelineConfig config;
            config.channel_id = ch.channel_id;
            config.stream_url = ch.stream_url;
            config.reconnect_delay = 3;
            config.max_reconnect_attempts = -1;
            config.decoder_threads = 2;
            config.raw_queue_size = 64;
            config.decoded_queue_size = 16;
            config.processed_queue_size = 16;
            config.filters = ch.filters;
            config.enable_preprocess = true;
            config.target_width = ch.target_width;
            config.target_height = ch.target_height;
            
            bool success = manager.addStream(ch.channel_id, config);
            if (success) {
                std::cout << "Added channel " << ch.channel_id 
                          << ": " << ch.stream_url << std::endl;
            }
            else {
                std::cerr << "Failed to add channel " << ch.channel_id << std::endl;
            }
        }
        
        std::cout << "\nTotal channels: " << manager.getTotalCount() << std::endl;
        std::cout << "Press Ctrl+C to stop...\n" << std::endl;
        
        // 创建算法处理器（每个通道独立）
        std::map<int, std::unique_ptr<IAlgorithm>> algorithms;
        for (const auto& ch : channels) {
            algorithms[ch.channel_id] = std::make_unique<MotionDetectionAlgorithm>();
        }
        
        // 创建输出器
        std::vector<std::shared_ptr<IResultOutput>> outputs;
        outputs.push_back(std::make_shared<ConsoleOutput>());
        outputs.push_back(std::make_shared<LogOutput>());
        
        // 创建 OSD 渲染器（每个通道独立）
        std::map<int, std::unique_ptr<OsdRenderer>> osd_renderers;
        for (const auto& ch : channels) {
            osd_renderers[ch.channel_id] = std::make_unique<OsdRenderer>();
        }
        
        // 设置全局帧输出回调
        std::map<int, int> processed_counts;
        std::map<int, std::chrono::steady_clock::time_point> start_times;
        
        for (const auto& ch : channels) {
            processed_counts[ch.channel_id] = 0;
            start_times[ch.channel_id] = std::chrono::steady_clock::now();
        }
        
        manager.setGlobalFrameCallback(
            [&algorithms, &outputs, &osd_renderers, 
             &processed_counts, &start_times](
                int channel_id, cv::Mat&& frame, int64_t pts) {
                
                // 更新计数和时间
                processed_counts[channel_id]++;
                auto& start_time = start_times[channel_id];
                
                // 计算 FPS
                auto now = std::chrono::steady_clock::now();
                double elapsed_sec = std::chrono::duration<double>(now - start_time).count();
                float fps = (elapsed_sec > 0) ? 
                    static_cast<float>(processed_counts[channel_id] / elapsed_sec) : 0.0f;
                
                // 运行算法
                AlgorithmResult result = algorithms[channel_id]->process(frame, channel_id, pts);
                
                // 构建检测框列表
                std::vector<std::tuple<int, int, int, int, std::string, float>> detection_boxes;
                if (result.confidence > 0.1f) {
                    int box_x = frame.cols / 4;
                    int box_y = frame.rows / 4;
                    int box_w = frame.cols / 2;
                    int box_h = frame.rows / 2;
                    detection_boxes.emplace_back(box_x, box_y, box_w, box_h, 
                                               "Motion", result.confidence);
                }
                
                // 使用 OSD 渲染器绘制信息
                osd_renderers[channel_id]->render(frame, channel_id, pts, fps, detection_boxes);
                
                // 显示窗口（每 3 帧更新一次）
                static std::map<int, int> display_counters;
                if (++display_counters[channel_id] % 3 == 0) {
                    cv::imshow("Channel " + std::to_string(channel_id), frame);
                    int key = cv::waitKey(1);
                    
                    if (key == 27 || key == 'q' || key == 'Q') {
                        g_running = false;
                    }
                }
                
                // 输出结果
                for (auto& output : outputs) {
                    output->output(result);
                }
                
                // 每 100 帧打印统计
                if (processed_counts[channel_id] % 100 == 0) {
                    std::cout << "[Channel " << channel_id 
                              << "] Processed=" << processed_counts[channel_id]
                              << ", FPS=" << fps << std::endl;
                }
            }
        );
        
        // 启动所有流水线
        manager.startAllStreams();
        
        std::cout << "All pipelines started!" << std::endl;
        
        // 在后台线程中运行 io_context
        std::thread io_thread([&io_ctx]() {
            std::cout << "[IO Thread] Running io_context..." << std::endl;
            io_ctx.run();
            std::cout << "[IO Thread] io_context stopped." << std::endl;
        });
        
        // 主循环：定期打印统计信息
        while (g_running && manager.getRunningCount() > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // 获取所有通道的统计信息
            auto all_stats = manager.getAllStats();
            
            std::cout << "\n========== Pipeline Statistics ==========" << std::endl;
            for (const auto& stats : all_stats) {
                std::cout << "Channel " << stats.channel_id << ": "
                          << "Running=" << (stats.is_running ? "Yes" : "No")
                          << ", Received=" << stats.frames_received
                          << ", Decoded=" << stats.frames_decoded
                          << ", Processed=" << stats.frames_processed
                          << std::endl;
            }
            std::cout << "Running: " << manager.getRunningCount() 
                      << " / " << manager.getTotalCount()
                      << std::endl;
            std::cout << "==========================================\n" << std::endl;
        }
        
        // 停止所有流水线
        std::cout << "\nStopping all pipelines..." << std::endl;
        manager.stopAllStreams();
        
        // 关闭所有 OpenCV 窗口
        cv::destroyAllWindows();
        
        // 停止 io_context
        io_ctx.stop();
        
        // 等待 io 线程结束
        if (io_thread.joinable()) {
            io_thread.join();
        }
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test completed!" << std::endl;
        
        // 打印最终统计
        auto final_stats = manager.getAllStats();
        for (const auto& stats : final_stats) {
            std::cout << "Channel " << stats.channel_id 
                      << ": Processed=" << stats.frames_processed << std::endl;
        }
        
        std::cout << "========================================" << std::endl;
        
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
