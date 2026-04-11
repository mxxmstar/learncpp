#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>
#include <vector>
#include <string>
#include <map>
#include "log/logger.h"  // 包含 LoggerConfig 和 RotationPolicy
#include "net/httpclientpool.h"  // 包含 HttpClientPool::Config


struct ServerConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    int threads = 4;
};

/// @brief 客户端池配置（支持多实例）
struct ClientPoolConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 8888;
    std::size_t init_size = 5;
    std::size_t max_size = 20;
    int connect_timeout_ms = 30000;
    int idle_timeout_sec = 300;
    std::size_t max_requests_per_client = 100;
    
    /// @brief 转换为 HttpClientPool::Config
    Net::HttpClientPool::Config toHttpClientPoolConfig() const {
        Net::HttpClientPool::Config config;
        config.host = host;
        config.port = port;
        config.init_size = init_size;
        config.max_size = max_size;
        config.connect_timeout_ms = connect_timeout_ms;
        config.idle_timeout_sec = idle_timeout_sec;
        config.max_requests_per_client = max_requests_per_client;
        return config;
    }
};

// TODO: 增加模块日志配置
struct LogConfig {
    std::string level = "info";
    std::string dir = "./logs";
    std::string rotation = "daily";
    size_t max_file_size_mb = 100;
    size_t max_files = 5;    
    bool console = true;
    bool json_format = false;
    
    /// @brief 转换为 LoggerConfig
    /// @param logger_name 日志器名称
    /// @return LoggerConfig 对象
    LoggerConfig toLoggerConfig(const std::string& logger_name = "main") const {
        LoggerConfig config(logger_name, parseLevel(level));
        config.log_dir = dir;
        config.policy = parseRotation(rotation);
        config.max_file_size_mb = max_file_size_mb;
        config.max_files = max_files;
        config.write_to_console = console;
        config.is_json = json_format;
        return config;
    }

private:
    /// @brief 解析日志级别字符串
    static spdlog::level::level_enum parseLevel(const std::string& level_str) {
        if (level_str == "trace") return spdlog::level::trace;
        if (level_str == "debug") return spdlog::level::debug;
        if (level_str == "info") return spdlog::level::info;
        if (level_str == "warn") return spdlog::level::warn;
        if (level_str == "error") return spdlog::level::err;
        if (level_str == "critical") return spdlog::level::critical;
        return spdlog::level::info;  // 默认
    }
    
    /// @brief 解析滚动策略字符串
    static RotationPolicy parseRotation(const std::string& rotation_str) {
        if (rotation_str == "daily") {
            return RotationPolicy::DAILY;
        }
		else if (rotation_str == "filesize") {
			return RotationPolicy::FILESIZE;
        }
        return RotationPolicy::DAILY;  // 默认
    }
};

struct ThreadPoolConfig {
    int min_threads = 2;
    int max_threads = 8;
    int queue_size = 1000;
};

struct ZlmConfig {
    std::string zlm_host = "127.0.0.1";
    int zlm_port = 8888;
    std::string secret = "";
    bool debug_terminal = true;
    int rtmp_port = 1935;
    int rtsp_port = 554;
};

struct WebSocketConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 8081;
    int heartbeat_interval = 10;
    int timeout = 30;
};

struct CameraDbConfig {
    std::string db_path = "./data/camera.db";
    std::string host = "localhost";
    int port = 3306;
    std::string name = "cameras";
    std::string user = "root";
    std::string password = "";
    int pool_size = 10;
};

struct UserDbConfig {
    std::string db_path = "./data/user.db";
    std::string host = "localhost";
    int port = 3306;
    std::string name = "users";
    std::string user = "root";
    std::string password = "";
    int pool_size = 10;
};

struct AppConfig {
    ServerConfig server;
    ClientPoolConfig zlm_client;  ///< ZLM 客户端池配置
    std::map<std::string, LogConfig> logs;
    ZlmConfig zlm;
    WebSocketConfig websocket;
    CameraDbConfig camera_db;       ///< 摄像头数据库配置
    UserDbConfig user_db;         ///< 用户数据库配置
};

class ConfigManager {
public:
    using ConfigChangeCallback = std::function<void(const AppConfig&)>;

    static ConfigManager& getInstance();

    bool load(const std::string& config_path);
    bool reload();
    bool save(const std::string& config_path = "");

    const AppConfig& getConfig() const;
    AppConfig& getConfig();

    template<typename T>
    T get(const std::string& key, const T& default_value = T{}) const;

    bool validate() const;
    std::vector<std::string> getValidationErrors() const;

    void setChangeCallback(ConfigChangeCallback callback);
    void checkAndReload();

    std::string getConfigPath() const { return config_path_; }

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void applyDefaults();
    void parseConfig(const YAML::Node& node);
    YAML::Node toYaml() const;

    AppConfig config_;
    std::string config_path_;
    std::filesystem::file_time_type last_write_time_;
    mutable std::mutex mutex_;
    ConfigChangeCallback change_callback_;
};

namespace config_utils {
    spdlog::level::level_enum parseLogLevel(const std::string& level);
    std::string logLevelToString(spdlog::level::level_enum level);
}
