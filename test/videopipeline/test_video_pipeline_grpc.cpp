/**
 * VideoPipeline gRPC 闆嗘垚娴嬭瘯
 * 
 * 娴嬭瘯鐩爣锛?
 * 1. 楠岃瘉 GrpcVideoSender 鍦?VideoPipeline 涓殑闆嗘垚
 * 2. 娴嬭瘯瑙嗛甯т粠瑙ｇ爜鍒?gRPC 鍙戦€佺殑瀹屾暣娴佺▼
 * 3. 楠岃瘉 Python 绔兘鎺ユ敹骞舵樉绀鸿棰戞祦
 */

#include "video_pipeline.h"
#include "pipeline_config.h"
#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include "log/logmanager.h"

// ==================== Graceful Shutdown ====================
std::atomic<bool> g_running{true};
std::atomic<bool> g_shutdown_requested{false};

#ifdef _WIN32
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            if (!g_shutdown_requested.exchange(true)) {
                std::cout << "\n[Shutdown] Received shutdown signal, stopping gracefully..." << std::endl;
                g_running = false;
            }
            return TRUE;
        default:
            return FALSE;
    }
}
#else
void SignalHandler(int signum) {
    if (!g_shutdown_requested.exchange(true)) {
        std::cout << "\n[Shutdown] Received signal " << signum << ", stopping gracefully..." << std::endl;
        g_running = false;
    }
}
#endif

void SetupGracefulShutdown() {
#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#else
    signal(SIGINT, SignalHandler);   // Ctrl+C
    signal(SIGTERM, SignalHandler);  // kill command
#endif
}

int main(int argc, char* argv[]) {
    // 鍒濆鍖栨棩蹇?
    LogManager& log_mgr = LogManager::getInstance();
    log_mgr.Init();

    std::cout << "\n" << std::string(70, '#') << std::endl;
    std::cout << "# VideoPipeline gRPC Integration Test" << std::endl;
    std::cout << "# Testing GrpcVideoSender in VideoPipeline" << std::endl;
    std::cout << std::string(70, '#') << std::endl;
    
    // 璁剧疆浼橀泤鍏抽棴
    SetupGracefulShutdown();
    
    // ==================== 閰嶇疆鍙傛暟 ====================
    std::string stream_url = "http://127.0.0.1/live/proxy_cam1.live.flv";
    std::string grpc_server = "localhost:50053";
    int channel_id = 1;
    int test_duration_sec = 800;  // 娴嬭瘯鎸佺画鏃堕棿锛堢锛?
    
    // 瑙ｆ瀽鍛戒护琛屽弬鏁?
    if (argc > 1) {
        stream_url = argv[1];
    }
    if (argc > 2) {
        grpc_server = argv[2];
    }
    if (argc > 3) {
        test_duration_sec = std::atoi(argv[3]);
    }
    
    std::cout << "\nTest Configuration:" << std::endl;
    std::cout << "  Stream URL: " << stream_url << std::endl;
    std::cout << "  gRPC Server: " << grpc_server << std::endl;
    std::cout << "  Channel ID: " << channel_id << std::endl;
    std::cout << "  Test Duration: " << test_duration_sec << "s" << std::endl;
    
    // ==================== 鍒涘缓閰嶇疆 ====================
    PipelineConfig config;
    config.stream_url = stream_url;
    config.channel_id = channel_id;
    
    // 鍚敤 gRPC 鍙戦€?
    config.enable_grpc_send = true;
    config.grpc_server_address = grpc_server;
    config.grpc_target_fps = 10;
    
    // 鍏朵粬閰嶇疆
    config.reconnect_delay = 3;
    config.max_reconnect_attempts = -1;  // 鏃犻檺閲嶈瘯
    config.decoder_threads = 2;
    config.raw_queue_size = 64;
    config.decoded_queue_size = 16;
    
    std::cout << "\nPipeline Config:" << std::endl;
    std::cout << "  Enable gRPC: " << (config.enable_grpc_send ? "YES" : "NO") << std::endl;
    std::cout << "  Decoder threads: " << config.decoder_threads << std::endl;
    std::cout << "  Queue sizes: raw=" << config.raw_queue_size 
              << ", decoded=" << config.decoded_queue_size << std::endl;
    
    // ==================== 鍒涘缓 io_context ====================
    boost::asio::io_context io_ctx;
    
    // 鍦ㄥ悗鍙扮嚎绋嬭繍琛?io_context
    std::thread io_thread([&io_ctx]() {
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work(io_ctx.get_executor());
        io_ctx.run();
    });
    
    // ==================== 鍒涘缓 VideoPipeline ====================
    std::cout << "\nCreating VideoPipeline..." << std::endl;
    auto pipeline = std::make_unique<VideoPipeline>(io_ctx, config);
    
    // 璁剧疆杈撳嚭鍥炶皟锛堝彲閫夛紝鐢ㄤ簬璋冭瘯锛?
    pipeline->setFrameOutputCallback([](int ch_id, cv::Mat&& frame, int64_t pts) {
        static int count = 0;
        if (++count % 30 == 0) {
            std::cout << "[Output] Channel " << ch_id << ": frame " << count 
                     << ", size=" << frame.cols << "x" << frame.rows << std::endl;
        }
    });
    
    // ==================== 鍚姩娴佹按绾?====================
    std::cout << "\nStarting VideoPipeline..." << std::endl;
    if (!pipeline->start()) {
        std::cerr << "Error: Failed to start VideoPipeline" << std::endl;
        std::cerr << "Please check:" << std::endl;
        std::cerr << "  1. Stream URL is correct and accessible" << std::endl;
        std::cerr << "  2. Python gRPC server is running on " << grpc_server << std::endl;
        return 1;
    }
    
    std::cout << "鉁?VideoPipeline started successfully" << std::endl;
    std::cout << "\nWaiting for frames..." << std::endl;
    std::cout << "(Press Ctrl+C to stop early)" << std::endl;
    
    // ==================== 杩愯娴嬭瘯 ====================
    auto start_time = std::chrono::steady_clock::now();
    int elapsed_sec = 0;
    
    while (g_running && elapsed_sec < test_duration_sec) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        elapsed_sec++;
        
        // 姣忕鎵撳嵃缁熻淇℃伅
        if (elapsed_sec % 5 == 0) {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                now - start_time).count();
            
            uint64_t frames_received = pipeline->getFramesReceived();
            uint64_t frames_decoded = pipeline->getFramesDecoded();
            uint64_t frames_processed = pipeline->getFramesProcessed();
            uint64_t grpc_sent = pipeline->getGrpcFramesSent();
            uint64_t grpc_failed = pipeline->getGrpcFramesFailed();
            
            std::cout << "\n--- Statistics at " << elapsed_sec << "s ---" << std::endl;
            std::cout << "  Received:  " << frames_received << " frames" << std::endl;
            std::cout << "  Decoded:   " << frames_decoded << " frames" << std::endl;
            std::cout << "  Processed: " << frames_processed << " frames" << std::endl;
            std::cout << "  gRPC Sent: " << grpc_sent << " frames" << std::endl;
            std::cout << "  gRPC Fail: " << grpc_failed << " frames" << std::endl;
            
            if (duration > 0) {
                std::cout << "  FPS (recv): " << (frames_received / duration) << std::endl;
                std::cout << "  FPS (grpc): " << (grpc_sent / duration) << std::endl;
            }
            
            double fail_rate = grpc_sent > 0 ? 
                (static_cast<double>(grpc_failed) / grpc_sent * 100.0) : 0.0;
            std::cout << "  Fail rate: " << fail_rate << "%" << std::endl;
        }
    }
    
    // ==================== 鍋滄娴佹按绾?====================
    std::cout << "\n[Shutdown] Stopping VideoPipeline..." << std::endl;
    
    // 1. 鍏堝仠姝?pipeline锛堜細鍋滄鎵€鏈夊唴閮ㄧ嚎绋嬪拰 gRPC 鍙戦€侊級
    pipeline->stop();
    std::cout << "[Shutdown] VideoPipeline stopped" << std::endl;
    
    // 2. 鍋滄 io_context
    io_ctx.stop();
    std::cout << "[Shutdown] IO context stopped" << std::endl;
    
    // 3. 绛夊緟 io_thread 缁撴潫
    if (io_thread.joinable()) {
        std::cout << "[Shutdown] Waiting for IO thread to finish..." << std::endl;
        io_thread.join();
        std::cout << "[Shutdown] IO thread joined" << std::endl;
    }
    
    std::cout << "[Shutdown] All resources released" << std::endl;
    
    // ==================== 鏈€缁堢粺璁?====================
    auto end_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time).count();
    
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "# Final Statistics" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    std::cout << "Test duration: " << total_duration << "s" << std::endl;
    std::cout << "\nFrame Statistics:" << std::endl;
    std::cout << "  Received:  " << pipeline->getFramesReceived() << std::endl;
    std::cout << "  Decoded:   " << pipeline->getFramesDecoded() << std::endl;
    std::cout << "  Processed: " << pipeline->getFramesProcessed() << std::endl;
    std::cout << "\ngRPC Statistics:" << std::endl;
    std::cout << "  Sent:      " << pipeline->getGrpcFramesSent() << std::endl;
    std::cout << "  Failed:    " << pipeline->getGrpcFramesFailed() << std::endl;
    
    uint64_t grpc_sent = pipeline->getGrpcFramesSent();
    uint64_t grpc_failed = pipeline->getGrpcFramesFailed();
    if (grpc_sent > 0) {
        double success_rate = (1.0 - static_cast<double>(grpc_failed) / grpc_sent) * 100.0;
        std::cout << "  Success:   " << success_rate << "%" << std::endl;
    }
    
    if (total_duration > 0) {
        std::cout << "\nPerformance:" << std::endl;
        std::cout << "  Avg recv FPS: " << (pipeline->getFramesReceived() / total_duration) << std::endl;
        std::cout << "  Avg grpc FPS: " << (pipeline->getGrpcFramesSent() / total_duration) << std::endl;
    }
    
    // ==================== 楠岃瘉缁撴灉 ====================
    std::cout << "\n" << std::string(70, '-') << std::endl;
    std::cout << "# Test Result" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    
    bool test_passed = true;
    
    if (pipeline->getGrpcFramesSent() == 0) {
        std::cout << "鉁?FAILED: No frames sent via gRPC" << std::endl;
        test_passed = false;
    } else {
        std::cout << "鉁?PASSED: Frames sent via gRPC: " << pipeline->getGrpcFramesSent() << std::endl;
    }
    
    if (pipeline->getGrpcFramesFailed() > pipeline->getGrpcFramesSent()) {
        std::cout << "鉁?WARNING: High failure rate" << std::endl;
    } else {
        std::cout << "鉁?PASSED: Acceptable failure rate" << std::endl;
    }
    
    if (pipeline->getFramesDecoded() == 0) {
        std::cout << "鉁?FAILED: No frames decoded" << std::endl;
        test_passed = false;
    } else {
        std::cout << "鉁?PASSED: Frames decoded: " << pipeline->getFramesDecoded() << std::endl;
    }
    
    std::cout << std::string(70, '-') << std::endl;
    if (test_passed) {
        std::cout << "# Overall: TEST PASSED 鉁? << std::endl;
    } else {
        std::cout << "# Overall: TEST FAILED 鉁? << std::endl;
    }
    std::cout << std::string(70, '-') << std::endl;
    
    return test_passed ? 0 : 1;
}

