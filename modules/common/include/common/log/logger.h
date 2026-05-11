#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

// 日志策略
enum class RotationPolicy {
    DAILY,
    FILESIZE,
    NONE    // 仅输出到主日志或控制台
};

struct LoggerConfig {
    LoggerConfig() = default;
    LoggerConfig(std::string name, spdlog::level::level_enum level = spdlog::level::info) : name(name), level(level) {}
    /// @brief 日志器名称
    std::string name;   
    /// @brief 日志级别
    spdlog::level::level_enum level = spdlog::level::info;
    /// @brief 日志目录
    std::string log_dir = "./logs";
    /// @brief 日志滚动策略
    RotationPolicy policy = RotationPolicy::DAILY;
    /// @brief 最大文件大小（MB）
    std::size_t max_file_size_mb = 100;
    /// @brief 最大文件数
    std::size_t max_files = 5;
    /// @brief 是否写入主日志
    bool write_to_main_log = true;
    /// @brief 是否写入控制台
    bool write_to_console = true;
    /// @brief 是否以 JSON 格式输出
    bool is_json = false;
    /// @brief 是否使用异步日志（默认true，调试时可设为false）
    bool async = true;
};

class Logger {
public:
    explicit Logger(const LoggerConfig& config);
    ~Logger() = default;

    std::shared_ptr<spdlog::logger> GetSpdLogger() { return spd_logger_; }

    void SetLevel(spdlog::level::level_enum level);

    spdlog::level::level_enum GetLevel() const;

    void SetFormat(const std::string& format);

    RotationPolicy GetRotationPolicy() const;

    std::size_t GetMaxFileSize() const;

    std::size_t GetMaxFiles() const;

    std::string GetLogDir() const;
    void Flush();
private:

    /// @brief 构建日志器的 sinks
    /// @param config 日志器配置
    /// @param sinks 日志器的 sinks
    void buildSinks(const LoggerConfig& config, std::vector<spdlog::sink_ptr>& sinks);

    std::shared_ptr<spdlog::logger> spd_logger_;
    LoggerConfig config_;
};
