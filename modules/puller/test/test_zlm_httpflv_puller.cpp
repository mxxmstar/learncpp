#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "puller/i_puller.h"
#include "puller/zlm/zlm_httpflv_puller.h"
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
    std::cout << "ZlmHttpFlvPuller Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 初始化日志管理器
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 创建 io_context
        boost::asio::io_context io_ctx;
        
        // 创建数据拉取器
        auto puller = std::make_unique<ZlmHttpFlvPuller>(io_ctx);
        
        // 设置重连参数
        puller->SetReconnectParams(3, -1);  // 3绉掑欢杩燂紝鏃犻檺閲嶈瘯
        
        std::string stream_url = "http://127.0.0.1/live/proxy_cam1.live.flv";
        
        std::cout << "Stream URL: " << stream_url << std::endl;
        std::cout << "Press Ctrl+C to stop...\n" << std::endl;
        
        // 统计信息
        int frame_count = 0;
        int64_t first_pts = -1;
        
        // 启动数据拉取器
        bool success = puller->Start(stream_url,
            // 序列头回调
            [&frame_count](int codec_id, const uint8_t* data, int size) {
                std::cout << "[Sequence Header] Codec=" << codec_id 
                          << ", Size=" << size << " bytes" << std::endl;
            },
            // 普通帧回调
            [&frame_count, &first_pts](const uint8_t* data, int size, int64_t pts) {
                // 统计信息
                if (first_pts < 0) {
                    first_pts = pts;
                }
                
                int64_t elapsed = pts - first_pts;
                
                // 每100帧输出一次统计信息
                if (frame_count % 100 == 0) {
                    std::cout << "[Frame " << frame_count 
                              << "] PTS=" << pts << "ms (" 
                              << (elapsed / 1000.0) << "s), "
                              << "Size=" << size << " bytes" << std::endl;
                }
                
                frame_count++;
            });
        
        if (!success) {
            std::cerr << "Failed to start puller" << std::endl;
            return 1;
        }
        
        // 启动 IO 线程
        std::thread io_thread([&io_ctx]() {
            io_ctx.run();
        });
        
        // 等待用户中断程序
        while (g_running && puller->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // 计算当前 FPS
            static int last_count = 0;
            int current_count = frame_count;
            int fps = (current_count - last_count) * 2;  // 姣忕閲囨牱 2 娆?
            last_count = current_count;
            
            std::cout << "[Stats] FPS=" << fps 
                      << ", TotalFrames=" << current_count
                      << std::endl;
        }
        
        // 停止数据拉取器
        std::cout << "\nStopping puller..." << std::endl;
        puller->Stop();
        
        // 等待 IO 线程结束
        io_thread.join();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test completed!" << std::endl;
        std::cout << "Total frames received: " << frame_count << std::endl;
        std::cout << "========================================" << std::endl;
        
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

