#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

// 日志级别枚举
    enum class LogLevel {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Err = 4,
        Critical = 5,
        Off = 6
    };

class LogManager {
public:
    static LogManager& getInstance() {
        static LogManager instance;
        return instance;
    }

    void Init(const std::string& base_dir = "logs");

    // 设置全局日志级别
    void setLogLevel(LogLevel level);

    // 获取不同模块的logger
    std::shared_ptr<spdlog::logger> getMainLogger() const { return main_logger_; }
    std::shared_ptr<spdlog::logger> getHttpLogger() const { return http_logger_; }
    std::shared_ptr<spdlog::logger> getRtspLogger() const { return rtsp_logger_; }
    std::shared_ptr<spdlog::logger> getRtmpLogger() const { return rtmp_logger_; }
    std::shared_ptr<spdlog::logger> getRtmpsLogger() const { return rtmps_logger_; }
    std::shared_ptr<spdlog::logger> getWebRtcLogger() const { return webrtc_logger_; }
    std::shared_ptr<spdlog::logger> getNetworkLogger() const { return network_logger_; }
    std::shared_ptr<spdlog::logger> getStreamLogger() const { return stream_logger_; }
private:    
    LogManager() = default;
    ~LogManager() = default;

    std::shared_ptr<spdlog::logger> main_logger_;
    std::shared_ptr<spdlog::logger> http_logger_;
    std::shared_ptr<spdlog::logger> rtsp_logger_;
    std::shared_ptr<spdlog::logger> rtmp_logger_;
    std::shared_ptr<spdlog::logger> rtmps_logger_;
    std::shared_ptr<spdlog::logger> webrtc_logger_;
    std::shared_ptr<spdlog::logger> network_logger_;
    std::shared_ptr<spdlog::logger> stream_logger_;
};

 // 便捷函数
    inline std::shared_ptr<spdlog::logger> getMainLogger() { return LogManager::getInstance().getMainLogger(); }
    inline std::shared_ptr<spdlog::logger> getHttpLogger() { return LogManager::getInstance().getHttpLogger(); }
    inline std::shared_ptr<spdlog::logger> getRtspLogger() { return LogManager::getInstance().getRtspLogger(); }
    inline std::shared_ptr<spdlog::logger> getRtmpLogger() { return LogManager::getInstance().getRtmpLogger(); }
    inline std::shared_ptr<spdlog::logger> getRtmpsLogger() { return LogManager::getInstance().getRtmpsLogger(); }
    inline std::shared_ptr<spdlog::logger> getWebRtcLogger() { return LogManager::getInstance().getWebRtcLogger(); }
    inline std::shared_ptr<spdlog::logger> getNetworkLogger() { return LogManager::getInstance().getNetworkLogger(); }
    inline std::shared_ptr<spdlog::logger> getStreamLogger() { return LogManager::getInstance().getStreamLogger(); }

    // 宏定义，便于使用
    #define LOG_MAIN spdlog::info
    #define LOG_HTTP SPDLOG_LOGGER_INFO(LogManager::getInstance().getHttpLogger().get())
    #define LOG_RTSP SPDLOG_LOGGER_INFO(LogManager::getInstance().getRtspLogger().get())
    #define LOG_RTMP SPDLOG_LOGGER_INFO(LogManager::getInstance().getRtmpLogger().get())
    #define LOG_RTMPS SPDLOG_LOGGER_INFO(LogManager::getInstance().getRtmpsLogger().get())
    #define LOG_WEBRTC SPDLOG_LOGGER_INFO(LogManager::getInstance().getWebRtcLogger().get())
    #define LOG_NETWORK SPDLOG_LOGGER_INFO(LogManager::getInstance().getNetworkLogger().get())
    #define LOG_STREAM SPDLOG_LOGGER_INFO(LogManager::getInstance().getStreamLogger().get())

    // 错误日志宏
    #define LOG_MAIN_ERROR spdlog::error
    #define LOG_HTTP_ERROR SPDLOG_LOGGER_ERROR(LogManager::getInstance().getHttpLogger().get())
    #define LOG_RTSP_ERROR SPDLOG_LOGGER_ERROR(LogManager::getInstance().getRtspLogger().get())
    #define LOG_RTMP_ERROR SPDLOG_LOGGER_ERROR(LogManager::getInstance().getRtmpLogger().get())
    #define LOG_RTMPS_ERROR SPDLOG_LOGGER_ERROR(LogManager::getInstance().getRtmpsLogger().get())
    #define LOG_WEBRTC_ERROR SPDLOG_LOGGER_ERROR(LogManager::getInstance().getWebRtcLogger().get())
    #define LOG_NETWORK_ERROR SPDLOG_LOGGER_ERROR(LogManager::getInstance().getNetworkLogger().get())
    #define LOG_STREAM_ERROR SPDLOG_LOGGER_ERROR(LogManager::getInstance().getStreamLogger().get())

    // 调试日志宏
    #define LOG_MAIN_DEBUG spdlog::debug
    #define LOG_HTTP_DEBUG SPDLOG_LOGGER_DEBUG(LogManager::getInstance().getHttpLogger().get())
    #define LOG_RTSP_DEBUG SPDLOG_LOGGER_DEBUG(LogManager::getInstance().getRtspLogger().get())
    #define LOG_RTMP_DEBUG SPDLOG_LOGGER_DEBUG(LogManager::getInstance().getRtmpLogger().get())
    #define LOG_RTMPS_DEBUG SPDLOG_LOGGER_DEBUG(LogManager::getInstance().getRtmpsLogger().get())
    #define LOG_WEBRTC_DEBUG SPDLOG_LOGGER_DEBUG(LogManager::getInstance().getWebRtcLogger().get())
    #define LOG_NETWORK_DEBUG SPDLOG_LOGGER_DEBUG(LogManager::getInstance().getNetworkLogger().get())
    #define LOG_STREAM_DEBUG SPDLOG_LOGGER_DEBUG(LogManager::getInstance().getStreamLogger().get())