#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

struct ServerConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    int threads = 4;
};

struct LogConfig {
    std::string level = "info";
    std::string dir = "./logs";
    size_t max_file_size_mb = 100;
    size_t max_files = 5;
    std::string rotation = "daily";
    bool console = true;
    bool json_format = false;
};

struct DatabaseConfig {
    bool enabled = false;
    std::string host = "localhost";
    int port = 3306;
    std::string name = "mydb";
    std::string user = "root";
    std::string password = "";
    int pool_size = 10;
};

struct ThreadPoolConfig {
    int min_threads = 2;
    int max_threads = 8;
    int queue_size = 1000;
};

struct MediaConfig {
    std::string zlm_host = "127.0.0.1";
    int zlm_port = 8888;
    std::string secret = "";
    int rtmp_port = 1935;
    int rtsp_port = 554;
};

struct WebSocketConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 8081;
    int heartbeat_interval = 10;
    int timeout = 30;
};

struct CameraConfig {
    std::string db_path = "./data/camera.db";
};

struct AppConfig {
    ServerConfig server;
    LogConfig log;
    DatabaseConfig database;
    ThreadPoolConfig thread_pool;
    MediaConfig media;
    WebSocketConfig websocket;
    CameraConfig camera;
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
