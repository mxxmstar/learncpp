#include "log/logger.h"

#include <filesystem>

void LogManager::Init(const std::string& base_dir) {
        // 创建日志目录
        std::filesystem::create_directories(base_dir);

        // 创建控制台sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::debug);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

        // 创建基础文件sink
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(base_dir + "/main.log", true);
        file_sink->set_level(spdlog::level::trace);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v");

        // 主日志器
        main_logger_ = std::make_shared<spdlog::logger>("main", spdlog::sinks_init_list{console_sink, file_sink});
        main_logger_->set_level(spdlog::level::trace);
        spdlog::register_logger(main_logger_);

        // HTTP协议日志器
        auto http_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            base_dir + "/http.log", 1024 * 1024 * 5, 3); // 5MB, 保留3个文件
        http_file_sink->set_level(spdlog::level::trace);
        http_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [HTTP] [%l] [thread %t] %v");
        http_logger_ = std::make_shared<spdlog::logger>("http", spdlog::sinks_init_list{console_sink, http_file_sink});
        http_logger_->set_level(spdlog::level::trace);
        spdlog::register_logger(http_logger_);

        // RTSP协议日志器
        auto rtsp_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            base_dir + "/rtsp.log", 1024 * 1024 * 5, 3);
        rtsp_file_sink->set_level(spdlog::level::trace);
        rtsp_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [RTSP] [%l] [thread %t] %v");
        rtsp_logger_ = std::make_shared<spdlog::logger>("rtsp", spdlog::sinks_init_list{console_sink, rtsp_file_sink});
        rtsp_logger_->set_level(spdlog::level::trace);
        spdlog::register_logger(rtsp_logger_);

        // RTMP协议日志器
        auto rtmp_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            base_dir + "/rtmp.log", 1024 * 1024 * 5, 3);
        rtmp_file_sink->set_level(spdlog::level::trace);
        rtmp_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [RTMP] [%l] [thread %t] %v");
        rtmp_logger_ = std::make_shared<spdlog::logger>("rtmp", spdlog::sinks_init_list{console_sink, rtmp_file_sink});
        rtmp_logger_->set_level(spdlog::level::trace);
        spdlog::register_logger(rtmp_logger_);

        // RTMPS协议日志器
        auto rtmps_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            base_dir + "/rtmps.log", 1024 * 1024 * 5, 3);
        rtmps_file_sink->set_level(spdlog::level::trace);
        rtmps_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [RTMPS] [%l] [thread %t] %v");
        rtmps_logger_ = std::make_shared<spdlog::logger>("rtmps", spdlog::sinks_init_list{console_sink, rtmps_file_sink});
        rtmps_logger_->set_level(spdlog::level::trace);
        spdlog::register_logger(rtmps_logger_);

        // WebRTC协议日志器
        auto webrtc_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            base_dir + "/webrtc.log", 1024 * 1024 * 5, 3);
        webrtc_file_sink->set_level(spdlog::level::trace);
        webrtc_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [WEBRTC] [%l] [thread %t] %v");
        webrtc_logger_ = std::make_shared<spdlog::logger>("webrtc", spdlog::sinks_init_list{console_sink, webrtc_file_sink});
        webrtc_logger_->set_level(spdlog::level::trace);
        spdlog::register_logger(webrtc_logger_);

        // 网络层日志器
        auto network_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            base_dir + "/network.log", 1024 * 1024 * 5, 3);
        network_file_sink->set_level(spdlog::level::trace);
        network_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [NETWORK] [%l] [thread %t] %v");
        network_logger_ = std::make_shared<spdlog::logger>("network", spdlog::sinks_init_list{console_sink, network_file_sink});
        network_logger_->set_level(spdlog::level::trace);
        spdlog::register_logger(network_logger_);

        // 流处理日志器
        auto stream_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            base_dir + "/stream.log", 1024 * 1024 * 5, 3);
        stream_file_sink->set_level(spdlog::level::trace);
        stream_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [STREAM] [%l] [thread %t] %v");
        stream_logger_ = std::make_shared<spdlog::logger>("stream", spdlog::sinks_init_list{console_sink, stream_file_sink});
        stream_logger_->set_level(spdlog::level::trace);
        spdlog::register_logger(stream_logger_);
    }

    void LogManager::setLogLevel(LogLevel level) {
        spdlog::level::level_enum spdlog_level = static_cast<spdlog::level::level_enum>(static_cast<int>(level));
        spdlog::set_level(spdlog_level);
        
        if (main_logger_) main_logger_->set_level(spdlog_level);
        if (http_logger_) http_logger_->set_level(spdlog_level);
        if (rtsp_logger_) rtsp_logger_->set_level(spdlog_level);
        if (rtmp_logger_) rtmp_logger_->set_level(spdlog_level);
        if (rtmps_logger_) rtmps_logger_->set_level(spdlog_level);
        if (webrtc_logger_) webrtc_logger_->set_level(spdlog_level);
        if (network_logger_) network_logger_->set_level(spdlog_level);
        if (stream_logger_) stream_logger_->set_level(spdlog_level);
    }