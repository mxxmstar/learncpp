#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "video_pipeline/puller/zlm_puller.h"
#include "video_pipeline/frame_queue.h"
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
    std::cout << "ZLMPuller Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 创建 io_context
        boost::asio::io_context io_ctx;
        
        // 创建拉流器
        auto puller = std::make_unique<ZLMPuller>(io_ctx);
        
        // 创建帧队列（用于接收拉流数据）
        auto queue = std::make_shared<RawPacketQueue>(64);
        
        // 配置参数
        std::string stream_url = "http://127.0.0.1:8080/live/test.flv";
        
        std::cout << "Stream URL: " << stream_url << std::endl;
        std::cout << "Press Ctrl+C to stop...\n" << std::endl;
        
        // 设置回调函数
        int frame_count = 0;
        int64_t first_pts = -1;
        
        puller->start(stream_url, 
            [&frame_count, &first_pts, queue](const uint8_t* data, int size, int64_t pts) {
                // 统计信息
                if (first_pts < 0) {
                    first_pts = pts;
                }
                
                int64_t elapsed = pts - first_pts;
                
                // 每 100 帧打印一次统计
                if (frame_count % 100 == 0) {
                    std::cout << "[Frame " << frame_count 
                              << "] PTS=" << pts << "ms (" 
                              << (elapsed / 1000.0) << "s), "
                              << "Size=" << size << " bytes" << std::endl;
                }
                
                frame_count++;
                
                // 推入队列（如果队列满则丢弃）
                RawPacketData packet(0, pts, data, size);
                if (!queue->push(std::move(packet))) {
                    // 队列已满，丢弃
                    static int dropped = 0;
                    if (++dropped % 100 == 0) {
                        std::cout << "[Warning] Queue full, dropped " << dropped << " frames\n";
                    }
                }
            });
        
        // 启动 io_context 在一个单独的线程中运行
        std::thread io_thread([&io_ctx]() {
            io_ctx.run();
        });
        
        // 主线程等待
        while (g_running && puller->isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // 打印统计信息
            static int last_count = 0;
            int current_count = frame_count;
            int fps = (current_count - last_count) * 2;  // 每秒采样 2 次
            last_count = current_count;
            
            std::cout << "[Stats] FPS=" << fps 
                      << ", QueueSize=" << queue->size()
                      << ", TotalFrames=" << current_count
                      << std::endl;
        }
        
        // 停止拉流器
        std::cout << "\nStopping puller..." << std::endl;
        puller->stop();
        
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
