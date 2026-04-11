#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "video_pipeline_manager.h"
#include "algorithm/base_algorithm.h"
#include "output/result_output.h"
#include "processor/osd_renderer.h"
#include "log/logmanager.h"

// 鍏ㄥ眬鏍囧織
std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    g_running = false;
}

int main() {
    // 璁剧疆淇″彿澶勭悊
    std::signal(SIGINT, signalHandler);
    
    std::cout << "========================================" << std::endl;
    std::cout << "Multi-Channel Video Processing Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 鍒濆鍖栨棩蹇?
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 鍒涘缓 io_context
        boost::asio::io_context io_ctx;
        
        // 鍒濆鍖栨祦姘寸嚎绠＄悊鍣?
        auto& manager = VideoPipelineManager::getInstance();
        manager.initialize(io_ctx);
        
        std::cout << "VideoPipelineManager initialized" << std::endl;
        
        // 閰嶇疆澶氫釜閫氶亾
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
        
        // 娣诲姞鎵€鏈夐€氶亾
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
        
        // 鍒涘缓绠楁硶澶勭悊鍣紙姣忎釜閫氶亾鐙珛锛?
        std::map<int, std::unique_ptr<IAlgorithm>> algorithms;
        for (const auto& ch : channels) {
            algorithms[ch.channel_id] = std::make_unique<MotionDetectionAlgorithm>();
        }
        
        // 鍒涘缓杈撳嚭鍣?
        std::vector<std::shared_ptr<IResultOutput>> outputs;
        outputs.push_back(std::make_shared<ConsoleOutput>());
        outputs.push_back(std::make_shared<LogOutput>());
        
        // 鍒涘缓 OSD 娓叉煋鍣紙姣忎釜閫氶亾鐙珛锛?
        std::map<int, std::unique_ptr<OsdRenderer>> osd_renderers;
        for (const auto& ch : channels) {
            osd_renderers[ch.channel_id] = std::make_unique<OsdRenderer>();
        }
        
        // 璁剧疆鍏ㄥ眬甯ц緭鍑哄洖璋?
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
                
                // 鏇存柊璁℃暟鍜屾椂闂?
                processed_counts[channel_id]++;
                auto& start_time = start_times[channel_id];
                
                // 璁＄畻 FPS
                auto now = std::chrono::steady_clock::now();
                double elapsed_sec = std::chrono::duration<double>(now - start_time).count();
                float fps = (elapsed_sec > 0) ? 
                    static_cast<float>(processed_counts[channel_id] / elapsed_sec) : 0.0f;
                
                // 杩愯绠楁硶
                AlgorithmResult result = algorithms[channel_id]->process(frame, channel_id, pts);
                
                // 鏋勫缓妫€娴嬫鍒楄〃
                std::vector<std::tuple<int, int, int, int, std::string, float>> detection_boxes;
                if (result.confidence > 0.1f) {
                    int box_x = frame.cols / 4;
                    int box_y = frame.rows / 4;
                    int box_w = frame.cols / 2;
                    int box_h = frame.rows / 2;
                    detection_boxes.emplace_back(box_x, box_y, box_w, box_h, 
                                               "Motion", result.confidence);
                }
                
                // 浣跨敤 OSD 娓叉煋鍣ㄧ粯鍒朵俊鎭?
                osd_renderers[channel_id]->render(frame, channel_id, pts, fps, detection_boxes);
                
                // 鏄剧ず绐楀彛锛堟瘡 3 甯ф洿鏂颁竴娆★級
                static std::map<int, int> display_counters;
                if (++display_counters[channel_id] % 3 == 0) {
                    cv::imshow("Channel " + std::to_string(channel_id), frame);
                    int key = cv::waitKey(1);
                    
                    if (key == 27 || key == 'q' || key == 'Q') {
                        g_running = false;
                    }
                }
                
                // 杈撳嚭缁撴灉
                for (auto& output : outputs) {
                    output->output(result);
                }
                
                // 姣?100 甯ф墦鍗扮粺璁?
                if (processed_counts[channel_id] % 100 == 0) {
                    std::cout << "[Channel " << channel_id 
                              << "] Processed=" << processed_counts[channel_id]
                              << ", FPS=" << fps << std::endl;
                }
            }
        );
        
        // 鍚姩鎵€鏈夋祦姘寸嚎
        manager.startAllStreams();
        
        std::cout << "All pipelines started!" << std::endl;
        
        // 鍦ㄥ悗鍙扮嚎绋嬩腑杩愯 io_context
        std::thread io_thread([&io_ctx]() {
            std::cout << "[IO Thread] Running io_context..." << std::endl;
            io_ctx.run();
            std::cout << "[IO Thread] io_context stopped." << std::endl;
        });
        
        // 涓诲惊鐜細瀹氭湡鎵撳嵃缁熻淇℃伅
        while (g_running && manager.getRunningCount() > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // 鑾峰彇鎵€鏈夐€氶亾鐨勭粺璁′俊鎭?
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
        
        // 鍋滄鎵€鏈夋祦姘寸嚎
        std::cout << "\nStopping all pipelines..." << std::endl;
        manager.stopAllStreams();
        
        // 鍏抽棴鎵€鏈?OpenCV 绐楀彛
        cv::destroyAllWindows();
        
        // 鍋滄 io_context
        io_ctx.stop();
        
        // 绛夊緟 io 绾跨▼缁撴潫
        if (io_thread.joinable()) {
            io_thread.join();
        }
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test completed!" << std::endl;
        
        // 鎵撳嵃鏈€缁堢粺璁?
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

