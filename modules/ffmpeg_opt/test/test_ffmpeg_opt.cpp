/**
 * @file test_ffmpeg_opt.cpp
 * @brief FFmpeg 推流工具测试
 */

#include "ffmpeg_opt/ffmpeg_opt.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "log/logmanager.h"
void testRTSPToRTMP() {
    std::cout << "\n=== Test 1: RTSP to RTMP ===" << std::endl;
    
    std::string rtsp_url = "rtsp://192.168.66.166/live/mainstream";
    std::string rtmp_url = "rtmp://127.0.0.1:1935/live/proxy_cam1";
    
    std::cout << "RTSP URL: " << rtsp_url << std::endl;
    std::cout << "RTMP URL: " << rtmp_url << std::endl;
    
    // 注意：这个测试会阻塞，直到 FFmpeg 进程结束
    // 实际使用时应该在单独的线程中运行
    bool success = FFmpegOpt::PushRTSPToRTMP(rtsp_url, rtmp_url, true);
    
    std::cout << "Result: " << (success ? "SUCCESS" : "FAILED") << std::endl;
}

void testUSBCameraNoAudio() {
    std::cout << "\n=== Test 2: USB Camera to RTMP (No Audio) ===" << std::endl;
    
#ifdef _WIN32
    std::string device_name = "USB2.0 UVC PC Camera";
#else
    std::string device_name = "/dev/video0";
#endif
    
    std::string rtmp_url = "rtmp://127.0.0.1:1935/live/proxy_cam1";
    
    std::cout << "Device: " << device_name << std::endl;
    std::cout << "RTMP URL: " << rtmp_url << std::endl;
    
    bool success = FFmpegOpt::PushUSBCameraToRTMP(
        device_name, 
        rtmp_url,
        15,   // fps
        640,  // width
        480   // height
    );
    
    std::cout << "Result: " << (success ? "SUCCESS" : "FAILED") << std::endl;
}

void testUSBCameraWithAudio() {
    std::cout << "\n=== Test 3: USB Camera to RTMP (With Audio) ===" << std::endl;
    
#ifdef _WIN32
    std::string video_device = "USB2.0 UVC PC Camera";
    std::string audio_device = "USB2.0 UVC PC Camera";
#else
    std::string video_device = "/dev/video0";
    std::string audio_device = "hw:0,0";  // ALSA 设备
#endif
    
    std::string rtmp_url = "rtmp://127.0.0.1:1935/live/proxy_cam1";
    
    std::cout << "Video Device: " << video_device << std::endl;
    std::cout << "Audio Device: " << audio_device << std::endl;
    std::cout << "RTMP URL: " << rtmp_url << std::endl;
    
    bool success = FFmpegOpt::PushUSBCameraWithAudioToRTMP(
        video_device,
        audio_device,
        rtmp_url,
        15,   // fps
        640,  // width
        480   // height
    );
    
    std::cout << "Result: " << (success ? "SUCCESS" : "FAILED") << std::endl;
}

void testGetFFmpegPath() {
    std::cout << "\n=== Test 4: Get FFmpeg Path ===" << std::endl;
    
    std::string path = FFmpegOpt::GetFFmpegPath();
    
    if (path.empty()) {
        std::cout << "FFmpeg not found!" << std::endl;
    } else {
        std::cout << "FFmpeg path: " << path << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "FFmpeg Opt Module Test" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 初始化日志
    LogManager& log_mgr = LogManager::getInstance();
    log_mgr.Init();

    // 测试 1: 获取 FFmpeg 路径
    //testGetFFmpegPath();
    
    // 测试 2: RTSP 转 RTMP（注释掉，需要实际的 RTSP 源）
     testRTSPToRTMP();
    
    // 测试 3: USB 摄像头无音频（注释掉，需要实际的摄像头）
    // testUSBCameraNoAudio();
    
    // 测试 4: USB 摄像头有音频（注释掉，需要实际的摄像头和麦克风）
    // testUSBCameraWithAudio();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All tests completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
