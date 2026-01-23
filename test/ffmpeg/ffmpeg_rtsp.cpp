#include "ffmpeg_opt/ffmpeg_opt.h"
#include <string>
#include <filesystem>
#include <log/logmanager.h>
#include <iostream>
int main() {    
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();
    std::cout << "LogManager initialized" << std::endl;
    std::cout << FFMPEG_PATH << std::endl;   
    if (!std::filesystem::exists(FFMPEG_PATH)) {        
        LOG_MAIN_ERROR_AT("FFmpegStream::PullRTSPStream, ffmpeg not found: {}", FFMPEG_PATH);
        return -1;
    } else {
        LOG_MAIN_INFO_AT("FFmpegStream::PullRTSPStream, ffmpeg found: {}", FFMPEG_PATH);
    }
    std::string in_url = "rtsp://192.168.66.166/live/mainstream";
    // FFmpegStream::PullRTSPStream(in_url, "rtsp://127.0.0.1/live/mainstream1"); 
}