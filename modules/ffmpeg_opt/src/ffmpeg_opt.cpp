#include "ffmpeg_opt/ffmpeg_opt.h"
#include "common/log/logmanager.h"
#include <sstream>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// FFmpeg 路径宏（由 CMake 定义）
#ifndef FFMPEG_PATH
#define FFMPEG_PATH ""
#endif

bool FFmpegOpt::PushRTSPToRTMP(const std::string& rtsp_url, 
                               const std::string& rtmp_url,
                               bool copy_video) {
    std::string command = buildRTSPToRTMPCommand(rtsp_url, rtmp_url, copy_video);
    
    if (command.empty()) {
        LOG_MAIN_ERROR_AT("FFmpegOpt::PushRTSPToRTMP - Failed to build command");
        return false;
    }
    
    LOG_MAIN_INFO_AT("FFmpegOpt::PushRTSPToRTMP - Command: {}", command);
    
    int result;
#ifdef _WIN32
    // Windows: 使用 CreateProcess 执行命令
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    
    // CreateProcess 需要可写的命令行缓冲区
    std::vector<char> cmd_buffer(command.begin(), command.end());
    cmd_buffer.push_back('\0');
    
    LOG_MAIN_DEBUG_AT("Executing with CreateProcess");
    
    BOOL success = CreateProcessA(
        NULL,                   // 应用程序名称（NULL 表示从命令行解析）
        cmd_buffer.data(),      // 命令行字符串
        NULL,                   // 进程安全属性
        NULL,                   // 线程安全属性
        FALSE,                  // 不继承句柄
        0,                      // 创建标志
        NULL,                   // 使用父进程的环境
        NULL,                   // 使用父进程的当前目录
        &si,                    // STARTUPINFO
        &pi                     // PROCESS_INFORMATION
    );
    
    if (!success) {
        DWORD error = GetLastError();
        LOG_MAIN_ERROR_AT("CreateProcess failed with error code: {}", error);
        return false;
    }
    
    // 等待进程结束
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    // 获取退出码
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    result = static_cast<int>(exitCode);
    
    // 清理句柄
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    LOG_MAIN_DEBUG_AT("Process exited with code: {}", result);
#else
    result = std::system(command.c_str());
#endif
    
    if (result == 0) {
        LOG_MAIN_INFO_AT("FFmpegOpt::PushRTSPToRTMP - Success");
        return true;
    } else {
        LOG_MAIN_ERROR_AT("FFmpegOpt::PushRTSPToRTMP - Failed with code: {}", result);
        return false;
    }
}

bool FFmpegOpt::PushUSBCameraToRTMP(const std::string& device_name,
                                    const std::string& rtmp_url,
                                    int fps,
                                    int width,
                                    int height) {
    // 无音频，传入 empty optional
    std::string command = buildUSBCameraCommand(device_name, std::nullopt, rtmp_url, fps, width, height);
    
    if (command.empty()) {
        LOG_MAIN_ERROR_AT("FFmpegOpt::PushUSBCameraToRTMP - Failed to build command");
        return false;
    }
    
    LOG_MAIN_INFO_AT("FFmpegOpt::PushUSBCameraToRTMP - Command: {}", command);
    
    int result = std::system(command.c_str());
    
    if (result == 0) {
        LOG_MAIN_INFO_AT("FFmpegOpt::PushUSBCameraToRTMP - Success");
        return true;
    } else {
        LOG_MAIN_ERROR_AT("FFmpegOpt::PushUSBCameraToRTMP - Failed with code: {}", result);
        return false;
    }
}

bool FFmpegOpt::PushUSBCameraWithAudioToRTMP(const std::string& video_device,
                                             const std::string& audio_device,
                                             const std::string& rtmp_url,
                                             int fps,
                                             int width,
                                             int height) {
    // 有音频，传入音频设备名
    std::string command = buildUSBCameraCommand(video_device, audio_device, rtmp_url, fps, width, height);
    
    if (command.empty()) {
        LOG_MAIN_ERROR_AT("FFmpegOpt::PushUSBCameraWithAudioToRTMP - Failed to build command");
        return false;
    }
    
    LOG_MAIN_INFO_AT("FFmpegOpt::PushUSBCameraWithAudioToRTMP - Command: {}", command);
    
    int result = std::system(command.c_str());
    
    if (result == 0) {
        LOG_MAIN_INFO_AT("FFmpegOpt::PushUSBCameraWithAudioToRTMP - Success");
        return true;
    } else {
        LOG_MAIN_ERROR_AT("FFmpegOpt::PushUSBCameraWithAudioToRTMP - Failed with code: {}", result);
        return false;
    }
}

std::string FFmpegOpt::GetFFmpegPath() {
    // 优先使用 CMake 定义的路径
    std::string cmake_path = FFMPEG_PATH;
    if (!cmake_path.empty() && std::filesystem::exists(cmake_path)) {
        LOG_MAIN_DEBUG_AT("FFmpegOpt::GetFFmpegPath - Using CMake defined path: {}", cmake_path);
        
#ifdef _WIN32
        // Windows: 手动转换为反斜杠，确保 cmd.exe 能正确解析
        std::string windows_path;
        windows_path.reserve(cmake_path.size());
        for (char c : cmake_path) {
            if (c == '/') {
                windows_path += '\\';
            } else {
                windows_path += c;
            }
        }
        LOG_MAIN_DEBUG_AT("FFmpegOpt::GetFFmpegPath - Windows path: {}", windows_path);
        return windows_path;
#else
        return cmake_path;
#endif
    }
    
    // 回退到默认路径
    std::filesystem::path exec_path;
#ifdef _WIN32
    exec_path = std::filesystem::path("tools") / "win32" / "ffmpeg-2025-05-01-git-707c04fe06-full_build" / "ffmpeg.exe";
#else
    exec_path = std::filesystem::path("tools") / "linux" / "ffmpeg_8_0" / "ffmpeg";
#endif
    
    // 转换为绝对路径
    std::filesystem::path parent_path = std::filesystem::current_path().parent_path();
    exec_path = parent_path / exec_path;
    
    if (std::filesystem::exists(exec_path)) {
        LOG_MAIN_DEBUG_AT("FFmpegOpt::GetFFmpegPath - Found at: {}", exec_path.string());
        return exec_path.string();
    }
    
    LOG_MAIN_WARN_AT("FFmpegOpt::GetFFmpegPath - FFmpeg not found");
    return "";
}

bool FFmpegOpt::ExecuteCommand(const std::string& command) {
    LOG_MAIN_INFO_AT("FFmpegOpt::ExecuteCommand - Command: {}", command);
    int result = std::system(command.c_str());
    return result == 0;
}

std::string FFmpegOpt::buildRTSPToRTMPCommand(const std::string& rtsp_url,
                                              const std::string& rtmp_url,
                                              bool copy_video) {
    std::string ffmpeg_path = GetFFmpegPath();
    if (ffmpeg_path.empty()) {
        LOG_MAIN_ERROR_AT("FFmpegOpt::buildRTSPToRTMPCommand - FFmpeg not found");
        return "";
    }
    
    std::ostringstream cmd;
    cmd << "\"" << ffmpeg_path << "\" ";
    
    // 输入参数：RTSP over TCP
    cmd << "-rtsp_transport tcp -i \"" << rtsp_url << "\" ";
    
    // 视频编码参数
    if (copy_video) {
        // 直接拷贝视频流（不重新编码，低延迟）
        cmd << "-c:v copy ";
    } else {
        // 重新编码为 H.264
        cmd << "-c:v libx264 -preset ultrafast -tune zerolatency ";
    }
    
    // 音频编码参数：AAC
    cmd << "-c:a aac ";
    
    // 输出格式：FLV (RTMP)
    cmd << "-f flv \"" << rtmp_url << "\"";
    
    return cmd.str();
}

std::string FFmpegOpt::buildUSBCameraCommand(const std::string& video_device,
                                             const std::optional<std::string>& audio_device,
                                             const std::string& rtmp_url,
                                             int fps,
                                             int width,
                                             int height) {
    std::string ffmpeg_path = GetFFmpegPath();
    if (ffmpeg_path.empty()) {
        LOG_MAIN_ERROR_AT("FFmpegOpt::buildUSBCameraCommand - FFmpeg not found");
        return "";
    }
    
    std::ostringstream cmd;
    cmd << "\"" << ffmpeg_path << "\" ";
    
#ifdef _WIN32
    // Windows: 使用 DirectShow
    cmd << "-f dshow -rtbufsize 100M -i video=\"" << video_device << "\"";
    
    // 如果有音频设备，添加音频输入
    if (audio_device.has_value() && !audio_device->empty()) {
        cmd << ":audio=\"" << *audio_device << "\"";
    }
#else
    // Linux: 使用 V4L2
    cmd << "-f v4l2 -i " << video_device;
    
    // Linux 音频通常使用 ALSA
    if (audio_device.has_value() && !audio_device->empty()) {
        cmd << " -f alsa -i " << *audio_device;
    }
#endif
    
    // 视频编码参数
    cmd << " -r " << fps 
        << " -s " << width << "x" << height
        << " -c:v libx264 -preset ultrafast -tune zerolatency ";
    
    // 音频编码参数
    if (audio_device.has_value() && !audio_device->empty()) {
        // 有音频：编码为 AAC
        cmd << "-c:a aac ";
    } else {
        // 无音频
        cmd << "-an ";
    }
    
    // 输出格式：FLV (RTMP)
    cmd << "-f flv \"" << rtmp_url << "\"";
    
    return cmd.str();
}
