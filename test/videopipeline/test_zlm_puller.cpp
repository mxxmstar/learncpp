#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "puller/zlm_puller.h"
#include "frame_queue.h"
#include "common/log/logmanager.h"

// 鍏ㄥ眬鏍囧織
std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    g_running = false;
}

int main() {
    // 璁剧疆淇″彿澶勭悊
    std::signal(SIGINT, signalHandler);
    // 鍒濆鍖栨棩蹇?
    LogManager& log_mgr = LogManager::getInstance();
    log_mgr.Init();
    std::cout << "========================================" << std::endl;
    std::cout << "ZLMPuller Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 鍒涘缓 io_context
        boost::asio::io_context io_ctx;
        
        // 鍒涘缓鎷夋祦鍣?
        auto puller = std::make_unique<ZLMPuller>(io_ctx);
        
        // 鍒涘缓甯ч槦鍒楋紙鐢ㄤ簬鎺ユ敹鎷夋祦鏁版嵁锛?
        auto queue = std::make_shared<RawPacketQueue>(64);
        
        // 閰嶇疆鍙傛暟
        std::string stream_url = "http://127.0.0.1/live/proxy_cam1.live.flv";
        
        std::cout << "Stream URL: " << stream_url << std::endl;
        std::cout << "Press Ctrl+C to stop...\n" << std::endl;
        
        // 璁剧疆鍥炶皟鍑芥暟
        int frame_count = 0;
        int64_t first_pts = -1;
        
        puller->start(stream_url,
            // 搴忓垪澶村洖璋?
            [&frame_count](int codec_id, const uint8_t* data, int size) {
                std::cout << "[Sequence Header] Codec=" << codec_id 
                          << ", Size=" << size << " bytes" << std::endl;
            },
            // 鏅€氬抚鍥炶皟
            [&frame_count, &first_pts, queue](const uint8_t* data, int size, int64_t pts) {
                // 缁熻淇℃伅
                if (first_pts < 0) {
                    first_pts = pts;
                }
                
                int64_t elapsed = pts - first_pts;
                
                // 姣?100 甯ф墦鍗颁竴娆＄粺璁?
                if (frame_count % 100 == 0) {
                    std::cout << "[Frame " << frame_count 
                              << "] PTS=" << pts << "ms (" 
                              << (elapsed / 1000.0) << "s), "
                              << "Size=" << size << " bytes" << std::endl;
                }
                
                frame_count++;
                
                // 鎺ㄥ叆闃熷垪锛堝鏋滈槦鍒楁弧鍒欎涪寮冿級
                RawPacketData packet(0, pts, data, size);
                if (!queue->push(std::move(packet))) {
                    // 闃熷垪宸叉弧锛屼涪寮?
                    static int dropped = 0;
                    if (++dropped % 100 == 0) {
                        std::cout << "[Warning] Queue full, dropped " << dropped << " frames\n";
                    }
                }
            });
        
        // 鍚姩 io_context 鍦ㄤ竴涓崟鐙殑绾跨▼涓繍琛?
        std::thread io_thread([&io_ctx]() {
            io_ctx.run();
        });
        
        // 涓荤嚎绋嬬瓑寰?
        while (g_running && puller->isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // 鎵撳嵃缁熻淇℃伅
            static int last_count = 0;
            int current_count = frame_count;
            int fps = (current_count - last_count) * 2;  // 姣忕閲囨牱 2 娆?
            last_count = current_count;
            
            std::cout << "[Stats] FPS=" << fps 
                      << ", QueueSize=" << queue->size()
                      << ", TotalFrames=" << current_count
                      << std::endl;
        }
        
        // 鍋滄鎷夋祦鍣?
        std::cout << "\nStopping puller..." << std::endl;
        puller->stop();
        
        // 绛夊緟 IO 绾跨▼缁撴潫
        if (io_thread.joinable()) {
            io_thread.join();
        }
        
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

