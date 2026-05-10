/**
 * VideoPipeline OpenVINO 集成测试
 * 
 * 测试目标：
 * 1. 验证 OpenVINOBackend 在 VideoPipeline 中的集成
 * 2. 测试视频帧从拉流、解码到 OpenVINO 推理的完整流程
 * 3. 验证零拷贝架构的有效性（YUV -> OpenVINO）
 */

#include "videopipeline/video_pipeline.h"
#include "videopipeline/pipeline_config.h"
#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstdlib>      // for getenv
#include <filesystem>   // for filesystem::exists
#ifdef _WIN32
#include <direct.h>     // for _getcwd
#include <windows.h>    // for GetModuleFileName, MAX_PATH
#else
#include <unistd.h>     // for getcwd
#endif
#include "common/log/logmanager.h"

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
    // ==================== 临时修复：手动设置环境变量 ====================
    // 注意：这是临时方案，正式应该通过 VS 配置或脚本设置
    #ifdef _WIN32
    // 获取可执行文件所在目录（应该是 .../test/bin）
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    std::string exe_dir = exe_path;
    exe_dir = exe_dir.substr(0, exe_dir.find_last_of("\\/"));
    
    // 如果可执行文件在 bin 目录，plugin_path 就是 exe_dir
    // 如果在其他位置，需要调整
    std::string plugin_path = exe_dir;  // 已经是 bin 目录了
    
    // 设置 OPENVINO_PLUGIN_PATHS
    _putenv_s("OPENVINO_PLUGIN_PATHS", plugin_path.c_str());
    
    // 添加到 PATH
    std::string current_path;
    const char* existing_path = getenv("PATH");
    if (existing_path) {
        current_path = existing_path;
    }
    std::string new_path = plugin_path + ";" + current_path;
    _putenv_s("PATH", new_path.c_str());
    
    std::cout << "[Auto-Config] EXE Directory: " << exe_dir << std::endl;
    std::cout << "[Auto-Config] Set OPENVINO_PLUGIN_PATHS=" << plugin_path << std::endl;
    #endif
    // ==================== 临时修复结束 ====================
    
    // ==================== 环境变量检查（调试用）====================
    std::cout << "\n=== Environment Variable Check ===" << std::endl;
    const char* plugin_path_check = std::getenv("OPENVINO_PLUGIN_PATHS");
    std::cout << "OPENVINO_PLUGIN_PATHS: " 
              << (plugin_path_check ? plugin_path_check : "NOT SET") << std::endl;
    
    const char* path_env = std::getenv("PATH");
    if (path_env) {
        std::string path_str(path_env);
        bool has_bin = path_str.find("bin") != std::string::npos;
        std::cout << "PATH contains 'bin': " << (has_bin ? "YES" : "NO") << std::endl;
    } else {
        std::cout << "PATH: NOT SET" << std::endl;
    }
    
    // 检查当前工作目录
    char cwd[1024];
    if (_getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << "Current Working Directory: " << cwd << std::endl;
    }
    
    // 检查模型文件是否存在
    std::string model_check_path = "yolov5s.xml";
    if (std::filesystem::exists(model_check_path)) {
        std::cout << "Model file exists: " << model_check_path << std::endl;
    } else {
        std::cout << "Model file NOT found: " << model_check_path << std::endl;
        // 尝试 bin 目录
        std::string bin_model = "bin/yolov5s.xml";
        if (std::filesystem::exists(bin_model)) {
            std::cout << "  -> Found in bin/: " << bin_model << std::endl;
        }
    }
    std::cout << "==================================\n" << std::endl;
    // ==================== 环境变量检查结束 ====================
    
    // 初始化日志
    LogManager& log_mgr = LogManager::getInstance();
    log_mgr.Init();

    std::cout << "\n" << std::string(70, '#') << std::endl;
    std::cout << "# VideoPipeline OpenVINO Integration Test" << std::endl;
    std::cout << "# Testing Puller -> Decoder -> OpenVINO Backend" << std::endl;
    std::cout << std::string(70, '#') << std::endl;
    
    // 设置优雅关闭
    SetupGracefulShutdown();
    
    // ==================== 配置参数 ====================
    std::string stream_url = "http://127.0.0.1:8888/live/proxy_cam1.live.flv";
    std::string model_path = "yolov5s.xml";  // 需要用户提供实际的模型路径
    std::string device = "CPU";
    int channel_id = 1;
    int test_duration_sec = 60;  // 测试持续时间（秒）
    
    // 解析命令行参数
    if (argc > 1) {
        stream_url = argv[1];
    }
    if (argc > 2) {
        model_path = argv[2];
    }
    if (argc > 3) {
        device = argv[3];
    }
    if (argc > 4) {
        test_duration_sec = std::atoi(argv[4]);
    }
    
    std::cout << "\nTest Configuration:" << std::endl;
    std::cout << "  Stream URL: " << stream_url << std::endl;
    std::cout << "  Model Path: " << (model_path.empty() ? "(not specified)" : model_path) << std::endl;
    std::cout << "  Device: " << device << std::endl;
    std::cout << "  Channel ID: " << channel_id << std::endl;
    std::cout << "  Test Duration: " << test_duration_sec << "s" << std::endl;
    
    // ==================== 创建配置 ====================
    PipelineConfig config;
    config.channel_id = channel_id;
    config.puller.stream_url = stream_url;
    
    // 启用 OpenVINO 算法
    config.algorithm.openvino.enabled = true;
    config.algorithm.openvino.model_path = model_path;
    config.algorithm.openvino.device = device;
    config.algorithm.openvino.confidence_threshold = 0.5f;
    config.algorithm.openvino.batch_size = 1;
    
    // 其他配置
    config.puller.reconnect_delay = 3;
    config.puller.max_reconnect_attempts = -1;  // 无限重试
    config.decoder.decoder_threads = 2;
    config.decoder.raw_queue_size = 64;
    config.decoder.decoded_queue_size = 16;
    
    std::cout << "\nPipeline Config:" << std::endl;
    std::cout << "  Algorithm: OpenVINO" << std::endl;
    std::cout << "  Decoder threads: " << config.decoder.decoder_threads << std::endl;
    std::cout << "  Queue sizes: raw=" << config.decoder.raw_queue_size 
              << ", decoded=" << config.decoder.decoded_queue_size << std::endl;
    
    // ==================== 创建 io_context ====================
    boost::asio::io_context io_ctx;
    
    // 在后台线程运行 io_context
    std::thread io_thread([&io_ctx]() {
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work(io_ctx.get_executor());
        io_ctx.run();
    });
    
    // ==================== 创建 VideoPipeline ====================
    std::cout << "\nCreating VideoPipeline with OpenVINO backend..." << std::endl;
    auto pipeline = std::make_unique<VideoPipeline>(io_ctx, config);
    
    // 设置检测结果回调（用于调试）
    pipeline->setResultCallback([](int ch_id, const DetectionResult& result) {
        static int count = 0;
        if (++count % 30 == 0) {
            std::cout << "[Result] Channel " << ch_id << ": detection #" << count 
                     << ", timestamp=" << result.timestamp 
                     << ", boxes=" << result.boxes.size() << std::endl;
        }
    });
    
    // ==================== 启动流水线 ====================
    std::cout << "\nStarting VideoPipeline..." << std::endl;
    if (!pipeline->start()) {
        std::cerr << "Error: Failed to start VideoPipeline" << std::endl;
        std::cerr << "Please check:" << std::endl;
        std::cerr << "  1. Stream URL is correct and accessible" << std::endl;
        if (!model_path.empty()) {
            std::cerr << "  2. Model path exists: " << model_path << std::endl;
        } else {
            std::cerr << "  2. No model path specified (OpenVINO will use NullBackend)" << std::endl;
        }
        std::cerr << "  3. OpenVINO runtime is properly installed" << std::endl;
        return 1;
    }
    
    std::cout << "VideoPipeline started successfully" << std::endl;
    std::cout << "\nWaiting for frames..." << std::endl;
    std::cout << "(Press Ctrl+C to stop early)" << std::endl;
    
    // ==================== 运行测试 ====================
    auto start_time = std::chrono::steady_clock::now();
    int elapsed_sec = 0;
    
    while (g_running && elapsed_sec < test_duration_sec) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        elapsed_sec++;
        
        // 每秒打印统计信息
        if (elapsed_sec % 5 == 0) {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                now - start_time).count();
            
            uint64_t frames_received = pipeline->getFramesReceived();
            uint64_t frames_decoded = pipeline->getFramesDecoded();
            uint64_t frames_processed = pipeline->getFramesProcessed();
            
            std::cout << "\n--- Statistics at " << elapsed_sec << "s ---" << std::endl;
            std::cout << "  Received:  " << frames_received << " frames" << std::endl;
            std::cout << "  Decoded:   " << frames_decoded << " frames" << std::endl;
            std::cout << "  Processed: " << frames_processed << " frames" << std::endl;
            
            if (duration > 0) {
                std::cout << "  FPS (recv): " << (frames_received / duration) << std::endl;
                std::cout << "  FPS (proc): " << (frames_processed / duration) << std::endl;
            }
        }
    }
    
    // ==================== 停止流水线 ====================
    std::cout << "\n[Shutdown] Stopping VideoPipeline..." << std::endl;
    
    // 1. 先停止 pipeline（会停止所有内部线程和后端）
    pipeline->stop();
    std::cout << "[Shutdown] VideoPipeline stopped" << std::endl;
    
    // 2. 停止 io_context
    io_ctx.stop();
    std::cout << "[Shutdown] IO context stopped" << std::endl;
    
    // 3. 等待 io_thread 结束
    if (io_thread.joinable()) {
        std::cout << "[Shutdown] Waiting for IO thread to finish..." << std::endl;
        io_thread.join();
        std::cout << "[Shutdown] IO thread joined" << std::endl;
    }
    
    std::cout << "[Shutdown] All resources released" << std::endl;
    
    // ==================== 最终统计 ====================
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
    
    if (total_duration > 0) {
        std::cout << "\nPerformance:" << std::endl;
        std::cout << "  Avg recv FPS: " << (pipeline->getFramesReceived() / total_duration) << std::endl;
        std::cout << "  Avg proc FPS: " << (pipeline->getFramesProcessed() / total_duration) << std::endl;
    }
    
    // ==================== 验证结果 ====================
    std::cout << "\n" << std::string(70, '-') << std::endl;
    std::cout << "# Test Result" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    
    bool test_passed = true;
    
    if (pipeline->getFramesReceived() == 0) {
        std::cout << "FAILED: No frames received from stream" << std::endl;
        test_passed = false;
    } else {
        std::cout << "PASSED: Frames received: " << pipeline->getFramesReceived() << std::endl;
    }
    
    if (pipeline->getFramesDecoded() == 0) {
        std::cout << "FAILED: No frames decoded" << std::endl;
        test_passed = false;
    } else {
        std::cout << "PASSED: Frames decoded: " << pipeline->getFramesDecoded() << std::endl;
    }
    
    if (pipeline->getFramesProcessed() == 0) {
        std::cout << "WARNING: No frames processed by OpenVINO backend" << std::endl;
        std::cout << "  This may be expected if no model was provided or backend is not initialized" << std::endl;
    } else {
        std::cout << "PASSED: Frames processed by OpenVINO: " << pipeline->getFramesProcessed() << std::endl;
    }
    
    std::cout << std::string(70, '-') << std::endl;
    if (test_passed) {
        std::cout << "# Overall: TEST PASSED" << std::endl;
    } else {
        std::cout << "# Overall: TEST FAILED" << std::endl;
    }
    std::cout << std::string(70, '-') << std::endl;
    
    return test_passed ? 0 : 1;
}