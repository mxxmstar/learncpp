/**
 * GrpcAlgorithmProcessor 集成测试
 * 
 * 测试流程：
 * 1. C++ 读取视频文件
 * 2. 通过 GrpcAlgorithmProcessor 发送到 Python gRPC 服务器
 * 3. Python 显示视频流并返回检测结果
 * 4. C++ 在回调中打印检测结果
 */

#include "alg/grpc/i_algorithm_processor.h"
#include "alg/grpc/grpc_to_alg.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

// using namespace video_pipeline::algorithm_processor;  // 旧的路径
// IAlgorithmProcessor 和 GrpcToAlg 在全局 namespace

// 全局标志
std::atomic<bool> g_running{true};

/**
 * @brief 检测结果回调
 */
void OnDetectionResult(const DetectionResult& result) {
    std::cout << "\n[Callback] Frame: " << result.frame_id << std::endl;
    std::cout << "  Algorithm: " << result.algorithm << std::endl;
    std::cout << "  Processing time: " << result.processing_time_ms << "ms" << std::endl;
    std::cout << "  Detected objects: " << result.boxes.size() << std::endl;
    
    for (size_t i = 0; i < result.boxes.size(); ++i) {
        const auto& box = result.boxes[i];
        std::cout << "    [" << i << "] " 
                 << box.class_name 
                 << " conf=" << box.confidence
                 << " pos=(" << box.x << "," << box.y << ")"
                 << " size=" << box.width << "x" << box.height
                 << std::endl;
    }
}

/**
 * @brief 编码 cv::Mat 为 JPEG
 */
std::vector<uint8_t> EncodeToJpeg(const cv::Mat& frame, int quality = 85) {
    std::vector<uchar> buf;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, quality};
    cv::imencode(".jpg", frame, buf, params);
    return std::vector<uint8_t>(buf.begin(), buf.end());
}

/**
 * @brief 主测试函数
 */
int main(int argc, char* argv[]) {
    std::cout << "\n" << std::string(70, '#') << std::endl;
    std::cout << "# GrpcAlgorithmProcessor Integration Test" << std::endl;
    std::cout << "# C++ → Python gRPC Video Stream + Detection" << std::endl;
    std::cout << std::string(70, '#') << std::endl;
    
    // 获取视频文件路径
    std::string video_path = "test.mp4"; // 默认视频
    if (argc > 1) {
        video_path = argv[1];
    }
    
    std::cout << "\nVideo file: " << video_path << std::endl;
    
    // 检查视频文件
    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) {
        std::cerr << "Error: Cannot open video file: " << video_path << std::endl;
        return 1;
    }
    
    int width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double fps = cap.get(cv::CAP_PROP_FPS);
    int total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    
    std::cout << "Video info:" << std::endl;
    std::cout << "  Resolution: " << width << "x" << height << std::endl;
    std::cout << "  FPS: " << fps << std::endl;
    std::cout << "  Total frames: " << total_frames << std::endl;
    
    // 创建算法处理器配置
    ProcessorConfig config;
    config.type = ProcessorType::GRPC_PYTHON;
    config.grpc_address = "localhost:50053";
    config.grpc_target_fps = 10; // 每秒发送 10 帧
    
    std::cout << "\nProcessor config:" << std::endl;
    std::cout << "  Type: gRPC Python" << std::endl;
    std::cout << "  Server: " << config.grpc_address << std::endl;
    std::cout << "  Target FPS: " << config.grpc_target_fps << std::endl;
    
    // 创建算法处理器
    auto processor = std::make_unique<GrpcToAlg>(config);
    
    // 设置检测结果回调
    processor->SetDetectionCallback(OnDetectionResult);
    
    // 启动处理器
    std::cout << "\nStarting algorithm processor..." << std::endl;
    if (!processor->Start()) {
        std::cerr << "Error: Failed to start algorithm processor" << std::endl;
        std::cerr << "Please make sure Python gRPC server is running:" << std::endl;
        std::cerr << "  cd algorithm/grpc_server" << std::endl;
        std::cerr << "  python video_service.py --port 50052" << std::endl;
        return 1;
    }
    
    std::cout << "Algorithm processor started" << std::endl;
    
    // 处理视频帧
    std::cout << "\nProcessing video frames..." << std::endl;
    std::cout << "(Press 'q' to quit)" << std::endl;
    
    int frame_count = 0;
    int sent_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    cv::Mat frame;
    while (g_running && cap.read(frame)) {
        frame_count++;
        
        // 编码为 JPEG
        auto jpeg_data = EncodeToJpeg(frame, 85);
        
        // 创建 VideoFrame
        VideoFrame video_frame;
        video_frame.data = std::move(jpeg_data);
        video_frame.width = frame.cols;
        video_frame.height = frame.rows;
        video_frame.frame_id = "frame_" + std::to_string(frame_count);
        video_frame.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // 发送到算法处理器
        if (processor->ProcessFrame(video_frame)) {
            sent_count++;
        }
        
        // 显示本地视频（可选）
        cv::imshow("Local Video", frame);
        
        // 控制帧率
        auto frame_interval = std::chrono::milliseconds(static_cast<int>(1000.0 / fps));
        std::this_thread::sleep_for(frame_interval);
        
        // 检查按键
        int key = cv::waitKey(1);
        if (key == 'q' || key == 27) { // ESC
            std::cout << "\nUser interrupted" << std::endl;
            break;
        }
        
        // 每 10 帧打印进度
        if (frame_count % 10 == 0) {
            std::cout << "Progress: " << frame_count << "/" << total_frames 
                     << " frames (sent: " << sent_count << ")" << std::endl;
        }
    }
    
    // 计算统计信息
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time).count();
    
    std::cout << "\n--- Processing Summary ---" << std::endl;
    std::cout << "Total frames read: " << frame_count << std::endl;
    std::cout << "Frames sent to Python: " << sent_count << std::endl;
    std::cout << "Duration: " << duration << "s" << std::endl;
    std::cout << "Read FPS: " << (duration > 0 ? frame_count / duration : 0) << std::endl;
    std::cout << "Send FPS: " << (duration > 0 ? sent_count / duration : 0) << std::endl;
    
    // 获取处理器统计
    auto stats = processor->GetStats();
    std::cout << "\n--- Processor Statistics ---" << std::endl;
    std::cout << "Frames processed: " << stats.frames_processed << std::endl;
    std::cout << "Frames failed: " << stats.frames_failed << std::endl;
    std::cout << "Avg processing time: " << stats.avg_processing_time_ms << "ms" << std::endl;
    std::cout << "Result FPS: " << stats.fps << std::endl;
    
    // 停止处理器
    std::cout << "\nStopping algorithm processor..." << std::endl;
    processor->Stop();
    
    // 清理
    cap.release();
    cv::destroyAllWindows();
    
    std::cout << "\nTest completed successfully!" << std::endl;
    
    return 0;
}
