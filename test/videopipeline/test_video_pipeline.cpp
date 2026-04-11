#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "video_pipeline.h"
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
    std::cout << "VideoPipeline Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 鍒濆鍖栨棩蹇?
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 鍒涘缓 io_context
        boost::asio::io_context io_ctx;
        
        // 閰嶇疆娴佹按绾?
        PipelineConfig config;
        config.channel_id = 1;
        config.stream_url = "http://127.0.0.1/live/proxy_cam1.live.flv";
        config.reconnect_delay = 3;
        config.max_reconnect_attempts = -1;  // 鏃犻檺閲嶈瘯
        config.decoder_threads = 2;
        config.raw_queue_size = 64;
        config.decoded_queue_size = 16;
        config.processed_queue_size = 16;
        
        // 娣诲姞婊ら暅閾?
        config.filters = {
            "hist_eq",        // 鐩存柟鍥惧潎琛″寲
            "gaussian_blur",  // 楂樻柉妯＄硦
            "grayscale"       // 鐏板害鍖?
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
        
        // 鍒涘缓娴佹按绾?
        VideoPipeline pipeline(io_ctx, config);
        
        // 璁剧疆杈撳嚭鍥炶皟锛堟帴鏀跺鐞嗗悗鐨勫抚锛?
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
                
                // 杩欓噷鍙互灏嗗抚浼犻€掔粰绠楁硶妯″潡
                // 鎴栬€呬繚瀛樺埌闃熷垪涓?
            }
        );
        
        // 鍚姩娴佹按绾?
        bool success = pipeline.start();
        if (!success) {
            std::cerr << "Failed to start pipeline" << std::endl;
            return 1;
        }
        
        std::cout << "Pipeline started successfully!" << std::endl;
        
        // 鍦ㄥ悗鍙扮嚎绋嬩腑杩愯 io_context锛堝鐞嗗紓姝ョ綉缁滄搷浣滐級
        std::thread io_thread([&io_ctx]() {
            std::cout << "[IO Thread] Running io_context..." << std::endl;
            io_ctx.run();
            std::cout << "[IO Thread] io_context stopped." << std::endl;
        });
        
        // 涓诲惊鐜瓑寰?
        while (g_running && pipeline.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 鎵撳嵃缁熻淇℃伅
            uint64_t received = pipeline.getFramesReceived();
            uint64_t decoded = pipeline.getFramesDecoded();
            uint64_t processed = pipeline.getFramesProcessed();
            
            std::cout << "[Stats] Received=" << received
                      << ", Decoded=" << decoded
                      << ", Processed=" << processed
                      << std::endl;
        }
        
        // 鍋滄娴佹按绾?
        std::cout << "\nStopping pipeline..." << std::endl;
        pipeline.stop();
        
        // 鍋滄 io_context
        io_ctx.stop();
        
        // 绛夊緟 io 绾跨▼缁撴潫
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

