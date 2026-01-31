#include "ffmpeg_opt/ffmpeg_opt.h"
#include <string>
#include <filesystem>
#include <log/logmanager.h>
#include <iostream>
#include "file_opt.h"
int main() {    
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();
    std::cout << "LogManager initialized" << std::endl;
    std::string in_url = "rtsp://192.168.66.166/live/mainstream";
    // FFmpegStream::ConvertRTSPToHLS(in_url, "D:/file_mx/aaaaa/code_mx/learncpp/hls/out.m3u8");


    std::string ffmpeg_path = R"(D:\file_mx\aaaaa\code_mx\learncpp\tools\win32\ffmpeg-2025-05-01-git-707c04fe06-full_build\ffmpeg.exe)";
    
    std::vector<std::string> args = {
        "-rtsp_transport", "tcp",
        "-i", "rtsp://192.168.66.166/live/mainstream",
        "-c:v", "libx264",
        "-c:a", "aac",
        "-f", "hls",
        "-hls_time", "6",
        "-hls_list_size", "6",
        "-hls_flags", "delete_segments",
        "-start_number", "0",
        "D:/file_mx/aaaaa/code_mx/learncpp/hls/out.m3u8"
    };

    try {
        int exit_code = FileOpt::Execute(ffmpeg_path, args, true);
        if (exit_code == 0) {
            std::cout << "FFmpeg executed successfully." << std::endl;
        } else {
            std::cerr << "FFmpeg failed with exit code: " << exit_code << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error launching FFmpeg: " << e.what() << std::endl;
        return 1;
    }
	return 0;
}