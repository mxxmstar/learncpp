/**
 * 视频处理 gRPC 客户端测试
 * 测试与 Python gRPC 服务端的通信
 */

#include "video_grpc_client.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

using namespace grpc_module;

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

// ========== 测试场景 1: 检测元数据 ==========

void TestDetectionStream() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Test 1: Detection Stream (Metadata)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    VideoGrpcClient client("localhost:50053");
    
    // 连接
    if (!client.Connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return;
    }
    
    // 统计信息
    int frame_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    // 启动检测流
    bool success = client.StartDetectionStream(
        [&frame_count](const std::string& frame_id, 
                      const std::vector<std::map<std::string, float>>& boxes,
                      int64_t processing_time_ms) {
            
            std::cout << "[Callback] Frame: " << frame_id 
                     << ", Boxes: " << boxes.size()
                     << ", Processing time: " << processing_time_ms << "ms" << std::endl;
            
            for (size_t i = 0; i < boxes.size() && i < 3; ++i) {
                const auto& box = boxes[i];
                std::cout << "  Box " << i << ": (" 
                         << box.at("x") << ", " << box.at("y") << ") "
                         << box.at("width") << "x" << box.at("height")
                         << " conf=" << box.at("confidence")
                         << std::endl;
            }
            
            frame_count++;
        }
    );
    
    if (!success) {
        std::cerr << "Failed to start detection stream" << std::endl;
        return;
    }
    
    // 创建测试视频帧
    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    
    std::cout << "\nSending frames... (Press Ctrl+C to stop)" << std::endl;
    
    // 发送帧
    while (g_running && frame_count < 100) {
        // 绘制移动的对象
        frame.setTo(cv::Scalar(0, 0, 0));
        
        int x = (frame_count * 5) % 640;
        int y = (frame_count * 3) % 480;
        cv::rectangle(frame, cv::Point(x, y), cv::Point(x + 50, y + 50), 
                     cv::Scalar(0, 255, 0), -1);
        
        // 添加帧号
        std::string text = "Frame " + std::to_string(frame_count);
        cv::putText(frame, text, cv::Point(10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 2);
        
        // 编码为 JPEG
        std::vector<uchar> buf;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 85};
        cv::imencode(".jpg", frame, buf);
        std::vector<uint8_t> frame_data(buf.begin(), buf.end());
        
        // 发送帧
        if (!client.SendFrameForDetection(frame_data, frame.cols, frame.rows)) {
            std::cerr << "Failed to send frame" << std::endl;
            break;
        }
        
        // 控制帧率（30 FPS）
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    
    // Graceful shutdown: 停止流并清理资源
    std::cout << "\n[Shutdown] Stopping detection stream..." << std::endl;
    client.StopDetectionStream();
    std::cout << "[Shutdown] Detection stream stopped" << std::endl;
    
    // 打印统计
    auto stats = client.GetStatistics();
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time).count();
    
    std::cout << "\n--- Statistics ---" << std::endl;
    std::cout << "Frames sent: " << stats.frames_sent << std::endl;
    std::cout << "Frames received: " << stats.frames_received << std::endl;
    std::cout << "Avg latency: " << stats.avg_latency_ms << "ms" << std::endl;
    std::cout << "Duration: " << duration << "s" << std::endl;
    std::cout << "FPS: " << (duration > 0 ? stats.frames_sent / duration : 0) << std::endl;
    
    // 断开连接
    std::cout << "[Shutdown] Disconnecting from server..." << std::endl;
    client.Disconnect();
    std::cout << "[Shutdown] Disconnected" << std::endl;
}

// ========== 测试场景 2: 处理后视频 ==========

void TestVideoProcessStream() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Test 2: Video Process Stream (Processed Video)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    VideoGrpcClient client("localhost:50053");
    
    // 连接
    if (!client.Connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return;
    }
    
    // 统计信息
    int frame_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    // 创建显示窗口
    cv::namedWindow("Processed Video", cv::WINDOW_AUTOSIZE);
    
    // 启动视频处理流
    bool success = client.StartVideoProcessStream(
        [&frame_count](const std::string& frame_id,
                      const std::vector<uint8_t>& frame_data,
                      int width,
                      int height,
                      int64_t processing_time_ms) {
            
            std::cout << "[Callback] Frame: " << frame_id 
                     << ", Size: " << width << "x" << height
                     << ", Data size: " << frame_data.size() << " bytes"
                     << ", Processing time: " << processing_time_ms << "ms" << std::endl;
            
            // 解码并显示处理后的帧
            if (!frame_data.empty()) {
                try {
                    cv::Mat processed_frame = cv::imdecode(frame_data, cv::IMREAD_COLOR);
                    if (!processed_frame.empty()) {
                        cv::imshow("Processed Video", processed_frame);
                        
                        // 检查按键
                        if (cv::waitKey(1) == 27) { // ESC
                            g_running = false;
                        }
                    }
                } catch (...) {
                    std::cerr << "Failed to decode frame" << std::endl;
                }
            }
            
            frame_count++;
        }
    );
    
    if (!success) {
        std::cerr << "Failed to start video process stream" << std::endl;
        return;
    }
    
    // 创建测试视频帧
    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    
    std::cout << "\nSending frames... (Press ESC or Ctrl+C to stop)" << std::endl;
    
    // 发送帧
    while (g_running && frame_count < 100) {
        // 绘制移动的对象
        frame.setTo(cv::Scalar(0, 0, 0));
        
        int x = (frame_count * 5) % 640;
        int y = (frame_count * 3) % 480;
        cv::rectangle(frame, cv::Point(x, y), cv::Point(x + 50, y + 50), 
                     cv::Scalar(0, 255, 0), -1);
        
        // 添加帧号
        std::string text = "Frame " + std::to_string(frame_count);
        cv::putText(frame, text, cv::Point(10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 2);
        
        // 编码为 JPEG
        std::vector<uchar> buf;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 85};
        cv::imencode(".jpg", frame, buf);
        std::vector<uint8_t> frame_data(buf.begin(), buf.end());
        
        // 发送帧
        if (!client.SendFrameForProcessing(frame_data, frame.cols, frame.rows)) {
            std::cerr << "Failed to send frame" << std::endl;
            break;
        }
        
        // 控制帧率（30 FPS）
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    
    // Graceful shutdown: 停止流并清理资源
    std::cout << "\n[Shutdown] Stopping video process stream..." << std::endl;
    client.StopVideoProcessStream();
    std::cout << "[Shutdown] Video process stream stopped" << std::endl;
    
    // 关闭所有窗口
    cv::destroyAllWindows();
    std::cout << "[Shutdown] OpenCV windows closed" << std::endl;
    
    // 打印统计
    auto stats = client.GetStatistics();
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time).count();
    
    std::cout << "\n--- Statistics ---" << std::endl;
    std::cout << "Frames sent: " << stats.frames_sent << std::endl;
    std::cout << "Frames received: " << stats.frames_received << std::endl;
    std::cout << "Avg latency: " << stats.avg_latency_ms << "ms" << std::endl;
    std::cout << "Duration: " << duration << "s" << std::endl;
    std::cout << "FPS: " << (duration > 0 ? stats.frames_sent / duration : 0) << std::endl;
    
    // 断开连接
    std::cout << "[Shutdown] Disconnecting from server..." << std::endl;
    client.Disconnect();
    std::cout << "[Shutdown] Disconnected" << std::endl;
}

// ========== 主函数 ==========

int main() {
    std::cout << "\n" << std::string(60, '#') << std::endl;
    std::cout << "# Video Processing gRPC Client Test" << std::endl;
    std::cout << "# Testing communication with Python gRPC Server" << std::endl;
    std::cout << std::string(60, '#') << std::endl;
    
    // 设置优雅关闭
    SetupGracefulShutdown();
    
    std::cout << "\nSelect test mode:" << std::endl;
    std::cout << "1. Detection Stream (Metadata only)" << std::endl;
    std::cout << "2. Video Process Stream (Processed video)" << std::endl;
    std::cout << "Enter choice (1 or 2): ";
    
    int choice;
    std::cin >> choice;
    
    switch (choice) {
        case 1:
            TestDetectionStream();
            break;
        case 2:
            TestVideoProcessStream();
            break;
        default:
            std::cerr << "Invalid choice" << std::endl;
            return 1;
    }
    
    std::cout << "\nTest completed!" << std::endl;
    return 0;
}
