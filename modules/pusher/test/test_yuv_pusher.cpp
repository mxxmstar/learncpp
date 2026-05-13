#include "pusher/i_pusher.h"
#include "common/log/logmanager.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <thread>

int main(int argc, char* argv[]) {
    LOG_MAIN_INFO_AT("=== YuvPusher Test ===");
    
    std::string rtsp_url = "rtsp://127.0.0.1:8554/live/test";
    int width = 640;
    int height = 480;
    int fps = 25;
    int duration_sec = 10;
    
    if (argc >= 2) rtsp_url = argv[1];
    if (argc >= 3) width = std::atoi(argv[2]);
    if (argc >= 4) height = std::atoi(argv[3]);
    if (argc >= 5) fps = std::atoi(argv[4]);
    if (argc >= 6) duration_sec = std::atoi(argv[5]);
    
    PusherConfig config;
    config.url = rtsp_url;
    config.width = width;
    config.height = height;
    config.fps = fps;
    config.bitrate = 1000;
    
    LOG_MAIN_INFO_AT("Config: url={}, {}x{} @ {}fps", config.url, width, height, fps);
    
    auto pusher = IPusher::Create();
    
    bool started = pusher->Start(config, [](bool success, const std::string& msg, const PusherStats& stats) {
        LOG_MAIN_INFO_AT("Callback: success={}, msg={}, sent={}, failed={}", 
                        success, msg, stats.frames_sent, stats.frames_failed);
    });
    
    if (!started) {
        LOG_MAIN_ERROR_AT("Failed to start pusher");
        return 1;
    }
    
    LOG_MAIN_INFO_AT("Pusher started, sending test frames for {} seconds...", duration_sec);
    
    cv::Mat test_frame(height, width, CV_8UC3);
    int64_t start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    int frame_count = 0;
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(duration_sec);
    
    while (std::chrono::steady_clock::now() < end_time && pusher->IsRunning()) {
        int64_t pts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() - start_time;
        
        test_frame = cv::Scalar(frame_count % 2 ? 100 : 200, 
                               frame_count % 2 ? 50 : 100, 
                               frame_count % 2 ? 50 : 100);
        cv::putText(test_frame, "Frame: " + std::to_string(frame_count), 
                   cv::Point(20, 40), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
        cv::putText(test_frame, "PTS: " + std::to_string(pts), 
                   cv::Point(20, 80), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
        
        pusher->PushFrame(test_frame, pts);
        frame_count++;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fps));
    }
    
    LOG_MAIN_INFO_AT("Sent {} frames, stopping...", frame_count);
    pusher->Stop();
    
    const auto& stats = pusher->GetStats();
    LOG_MAIN_INFO_AT("=== Test Complete ===");
    LOG_MAIN_INFO_AT("Frames sent: {}, failed: {}, rate: {:.1f}%", 
                    stats.frames_sent, stats.frames_failed, stats.getSuccessRate());
    
    return 0;
}