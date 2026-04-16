#pragma once
#include <string>
#include <optional>

/**
 * @brief FFmpeg 推流工具类
 * 
 * 提供常用的 FFmpeg 推流功能封装：
 * - RTSP 转 RTMP 推到 ZLMediaKit
 * - USB 摄像头推到 ZLMediaKit
 */
class FFmpegOpt {
public:
    /**
     * @brief RTSP 流转 RTMP 推到 ZLMediaKit
     * 
     * 命令示例：
     * ffmpeg.exe -rtsp_transport tcp -i rtsp://192.168.66.166/live/mainstream \
     *            -c:v copy -c:a aac -f flv rtmp://127.0.0.1:1935/live/proxy_cam1
     * 
     * @param rtsp_url RTSP 流地址
     * @param rtmp_url RTMP 输出地址
     * @param copy_video 是否直接拷贝视频流（不重新编码）
     * @return true 成功, false 失败
     */
    static bool PushRTSPToRTMP(const std::string& rtsp_url, 
                               const std::string& rtmp_url,
                               bool copy_video = true);
    
    /**
     * @brief USB 摄像头推 RTMP 到 ZLMediaKit（无音频）
     * 
     * 命令示例：
     * ffmpeg.exe -f dshow -rtbufsize 100M -i video="USB2.0 UVC PC Camera" \
     *            -r 15 -s 640x480 -c:v libx264 -preset ultrafast -tune zerolatency \
     *            -an -f flv rtmp://127.0.0.1:1935/live/proxy_cam1
     * 
     * @param device_name 设备名称（Windows: "USB2.0 UVC PC Camera", Linux: "/dev/video0"）
     * @param rtmp_url RTMP 输出地址
     * @param fps 帧率（默认 15）
     * @param width 宽度（默认 640）
     * @param height 高度（默认 480）
     * @return true 成功, false 失败
     */
    static bool PushUSBCameraToRTMP(const std::string& device_name,
                                    const std::string& rtmp_url,
                                    int fps = 15,
                                    int width = 640,
                                    int height = 480);
    
    /**
     * @brief USB 摄像头推 RTMP 到 ZLMediaKit（有音频）
     * 
     * 命令示例：
     * ffmpeg.exe -f dshow -rtbufsize 100M -i video="USB2.0 UVC PC Camera":audio="USB2.0 UVC PC Camera" \
     *            -r 15 -s 640x480 -c:v libx264 -preset ultrafast -tune zerolatency \
     *            -c:a aac -f flv rtmp://127.0.0.1:1935/live/proxy_cam1
     * 
     * @param video_device 视频设备名称
     * @param audio_device 音频设备名称（如果为空则不使用音频）
     * @param rtmp_url RTMP 输出地址
     * @param fps 帧率（默认 15）
     * @param width 宽度（默认 640）
     * @param height 高度（默认 480）
     * @return true 成功, false 失败
     */
    static bool PushUSBCameraWithAudioToRTMP(const std::string& video_device,
                                             const std::string& audio_device,
                                             const std::string& rtmp_url,
                                             int fps = 15,
                                             int width = 640,
                                             int height = 480);
    
    /**
     * @brief 获取 FFmpeg 可执行文件路径
     * @return FFmpeg 路径，如果未找到返回空字符串
     */
    static std::string GetFFmpegPath();
    
    /**
     * @brief 执行自定义 FFmpeg 命令
     * @param command 完整的 FFmpeg 命令
     * @return true 成功, false 失败
     */
    static bool ExecuteCommand(const std::string& command);

private:
    /**
     * @brief 构建 RTSP 转 RTMP 命令
     */
    static std::string buildRTSPToRTMPCommand(const std::string& rtsp_url,
                                              const std::string& rtmp_url,
                                              bool copy_video);
    
    /**
     * @brief 构建 USB 摄像头推流命令
     */
    static std::string buildUSBCameraCommand(const std::string& video_device,
                                             const std::optional<std::string>& audio_device,
                                             const std::string& rtmp_url,
                                             int fps,
                                             int width,
                                             int height);
};
