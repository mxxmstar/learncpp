#include "ffmpeg_opt/ffmpeg_opt.h"
#include "log/logmanager.h"
#include <sstream>
#include <filesystem>

bool FFmpegStream::ConvertRTSPToHLS(const std::string& rtsp_url, const std::string& output_url) {    
    std::string additional_params = "-hls_time 6 -hls_list_size 6 -hls_flags delete_segments -start_number 0";

    std::filesystem::path output_path(output_url);
    std::filesystem::path output_dir = output_path.parent_path();
    if (!std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
    }    
    return ConvertStream(rtsp_url, InputStreamType::RTSP, output_url, OutputStreamType::HLS, additional_params);
}

bool FFmpegStream::ConvertUSBCameraToHLS(const std::string& usb_camera_index, const std::string& output_url) {
    // std::string additional_params = "-f dshow -i video=\"USB2.0 UVC Camera\":audio=\"USB2.0 UVC Camera\"";
    // return ConvertStream(usb_camera_index, InputStreamType::USB_CAMERA, output_url, OutputStreamType::HLS, additional_params);
}

bool FFmpegStream::ConvertStream(const std::string& input_src, InputStreamType input_type, const std::string& output_dst, OutputStreamType output_type, const std::string& extra_params) { 
    std::string command = buildCommand(input_src, input_type, output_dst, output_type, extra_params);
    LOG_MAIN_INFO_AT("FFmpegStream::ConvertStream, command: {}", command);
    
    // 确保输出目录存在
    // HLS 输出会产生.m3u8播放列表和.ts片段文件，需要确保输出目录存在
    if (output_type == OutputStreamType::HLS || output_type == OutputStreamType::FILE) {
        std::filesystem::path output_path(output_dst);
        std::filesystem::path output_dir = output_path.parent_path();
        if (!std::filesystem::exists(output_dir)) {
            std::filesystem::create_directories(output_dir);
        }
    }    
    int result = std::system(command.c_str());
    return result == 0;
}

bool FFmpegStream::ExecuteCommand(const std::string& command) {
    LOG_MAIN_INFO_AT("FFmpegStream::ExecuteCommand, command: {}", command);
    int result = std::system(command.c_str());
    return result == 0;
}

std::string FFmpegStream::GetFFmpegPath() {
    std::filesystem::path exec_path;
#ifdef _WIN32
    exec_path = std::filesystem::path("tools") / "win32" / "ffmpeg-2025-05-01-git-707c04fe06-full_build" / "ffmpeg.exe";    
#else
    exec_path = std::filesystem::path("tools") / "linux" / "ffmpeg_8_0" / "ffmpeg";
#endif
    if (std::filesystem::exists(exec_path)) {
        return exec_path.string();
    }
    return "";
}

std::string FFmpegStream::buildCommand(const std::string& input_src, InputStreamType input_type, const std::string& output_dst, OutputStreamType output_type, const std::string& extra_params = "") {
    std::string ffmpeg_path = GetFFmpegPath();
    std::ostringstream cmd;
    cmd << "\"" << ffmpeg_path << "\" "
        << buildInputParams(input_src, input_type) << " "
        << buildOutputParams(output_dst, output_type, extra_params);
    return cmd.str();
}

std::string FFmpegStream::buildInputParams(const std::string& input_src, InputStreamType input_type) {
    std::ostringstream params;
    switch (input_type) {
        case InputStreamType::USB_CAMERA:
#ifdef _WIN32        
            params << "-f dshow -i video=\"" << input_src << "\"";
#else 
            params << "-f v4l2 -i " << input_src;
#endif
            break;
        case InputStreamType::FILE:
            params << "-i \"" << input_src << "\"";
            break;
        case InputStreamType::RTSP:
            params << "-rtsp_transport tcp -i \"" << input_src << "\"";
            break;
        case InputStreamType::RTMP:
            params << "-i \"" << input_src << "\"";
            break;        
        case InputStreamType::HLS:
            params << "-i \"" << input_src << "\"";
            break;
        case InputStreamType::UDP:
        case InputStreamType::TCP:
            params << "-i \"" << input_src << "\"";
            break;
        default:
            break;
    }
    return params.str();
}

std::string FFmpegStream::buildOutputParams(const std::string& output_dst, OutputStreamType output_type, const std::string& extra_params) {
    std::ostringstream params;
    switch (output_type) {
        // 分支1：本地文件输出（如MP4、AVI、MKV等）
        case OutputStreamType::FILE:
            // extra_params：自定义参数（如-c copy 直接拷贝流、-b:v 2M 设置视频码率）
            // "output_dst"：带引号的文件路径（避免路径含空格/特殊字符导致FFmpeg解析失败）
            params << extra_params << " \"" << output_dst << "\"";
            break;

        // 分支2：RTSP流输出（实时流媒体协议，常用于安防摄像头、直播）
        case OutputStreamType::RTSP:
            // -f rtsp：指定输出格式为RTSP（强制FFmpeg按RTSP协议封装数据）
            // extra_params：自定义RTSP参数（如-listen 1 监听RTSP端口、-rtsp_transport tcp 指定TCP传输）
            // "output_dst"：RTSP地址（如 rtsp://192.168.1.100:554/stream）
            params << "-f rtsp " << extra_params << " \"" << output_dst << "\"";
            break;

        // 分支3：RTMP流输出（实时消息传输协议，常用于直播平台、CDN）
        case OutputStreamType::RTMP:
            // -f flv：指定输出格式为FLV（RTMP流的底层封装格式，必须指定）
            // extra_params：自定义RTMP参数（如-b:a 128k 设置音频码率、-g 25 设置GOP大小）
            // "output_dst"：RTMP地址（如 rtmp://192.168.1.100:1935/live/stream）
            params << "-f flv " << extra_params << " \"" << output_dst << "\"";
            break;

        // 分支4：HLS流输出（HTTP直播流，常用于网页/移动端直播）
        case OutputStreamType::HLS:
            // -c:v libx264：视频编码器指定为H.264（HLS标准编码，兼容性最好）
            // -c:a aac：音频编码器指定为AAC（HLS标准音频编码）
            // -f hls：指定输出格式为HLS（强制按HLS协议生成m3u8索引文件和ts切片）
            // -hls_time 6：每个TS切片的时长为6秒（控制切片大小，越小延迟越低）
            // -hls_list_size 6：m3u8索引文件中最多保留6个切片（控制索引文件大小）
            // -hls_flags delete_segments：自动删除过期的TS切片（避免磁盘空间占用过大）
            // -start_number 0：TS切片文件的编号从0开始（便于后续索引和管理）
            // extra_params：自定义HLS参数（如-hls_segment_filename 指定切片命名规则）
            // "output_dst"：HLS输出路径（如 "D:/hls/out.m3u8"，会自动生成out.m3u8和out_0.ts等切片）
            params << "-c:v libx264 -c:a aac -f hls " << "-hls_time 6 "
                << "-hls_list_size 6 -hls_flags delete_segments "
                << "-start_number 0 " << extra_params << " \"" << output_dst << "\"";
            break;

        // 分支5：UDP/TCP网络流输出（实时传输，常用于本地局域网推流）
        case OutputStreamType::UDP:
        case OutputStreamType::TCP:
            // -f mpegts：指定输出格式为MPEG-TS（传输流，UDP/TCP流的标准封装格式，避免丢包导致画面花屏）
            // extra_params：自定义网络参数（如-buffer_size 1024000 设置网络缓存、-timeout 5000000 设置超时）
            // "output_dst"：UDP/TCP地址（如 udp://192.168.1.100:5000、tcp://192.168.1.100:8000）
            params << "-f mpegts " << extra_params << " \"" << output_dst << "\"";
            break;

        // 默认分支：无参数输出
        default:
            break;
    }
    return params.str();
}
