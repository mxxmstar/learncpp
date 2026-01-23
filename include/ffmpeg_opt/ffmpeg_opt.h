#pragma once
#include <string>

#ifdef _WIN32
#define FFMPEG_OPT_WINDOWS
extern const char *FFMPEG_PATH;
extern const char *FFPLAY_PATH;
extern const char *FFPROBE_PATH;
#else
#define FFMPEG_OPT_LINUX
extern const char *FFMPEG_PATH;
extern const char *FFPLAY_PATH;
extern const char *FFPROBE_PATH;
#endif
enum class InputStreamType {
    USB_CAMERA,
    FILE,
    RTSP,
    RTMP,
    HLS,
    UDP,
    TCP,
};

enum class OutputStreamType {
    FILE,
    RTSP,
    RTMP,
    HLS,
    UDP,
    TCP,
};

class FFmpegStream {
public:

    // 执行自定义FFmpeg命令
    static bool ExecuteCommand(const std::string& command);

    ///@brief 获取FFmpeg路径
    static std::string GetFFmpegPath();

    ///@brief 执行自定义FFmpeg命令
    static bool ExecuteCommand(const std::string& command);

    ///@brief 转换RTSP流为HLS流
    static bool ConvertRTSPToHLS(const std::string& rtsp_url, const std::string& output_url);

    static bool ConvertUSBCameraToHLS(const std::string& usb_camera_index, const std::string& output_url);
    ///@brief 转换输入流为输出流
    static bool ConvertStream(const std::string& input_src, InputStreamType input_type, 
        const std::string& output_dst, OutputStreamType output_type, const std::string& extra_params = "");

private:

    /**
     * @brief 构建FFmpeg输入流的命令行参数
     * @param input_src 输入源（文件路径/网络流地址，如 "D:/in.mp4"、"rtsp://192.168.1.100:554/stream"）
     * @param input_type 输入流类型（USB摄像头/文件/RTSP/RTMP/HLS/UDP/TCP）
     * @return 拼接好的FFmpeg输入参数字符串
     */
    static std::string buildInputParams(const std::string& input_src, InputStreamType input_type);
    /**
     * @brief 构建FFmpeg输出流的命令行参数
     * @param output_dst 输出目标（文件路径/网络流地址，如 "D:/out.mp4"、"rtmp://192.168.1.100:1935/live"）
     * @param output_type 输出流类型（文件/RTSP/RTMP/HLS/UDP/TCP）
     * @param extra_params 自定义额外参数（如码率 "-b:v 1M"、分辨率 "-s 1920x1080" 等）
     * @return 拼接好的FFmpeg输出参数字符串
     */
    static std::string buildOutputParams(const std::string& output_dst, OutputStreamType output_type, const std::string& extra_params = "");
    /**
     * @brief 构建FFmpeg完整命令行
     * @param input_src 输入源（文件路径/网络流地址）
     * @param input_type 输入流类型
     * @param output_dst 输出目标（文件路径/网络流地址）
     * @param output_type 输出流类型
     * @param extra_params 自定义额外参数（如码率 "-b:v 1M"、分辨率 "-s 1920x1080" 等）
     * @return 拼接好的FFmpeg完整命令行字符串
     */
    static std::string buildCommand(const std::string& input_src, InputStreamType input_type, const std::string& output_dst, OutputStreamType output_type, const std::string& extra_params = "");
    
};