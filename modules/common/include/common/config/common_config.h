#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <map>
#include <atomic>
#include <any>

#define DEBUG_MODE true

/// @brief 主http服务器配置
struct HttpServerConfig {
    std::string host = "127.0.0.1";
    int port = 8080;    
};

/// @brief 客户端池配置（支持多实例）
struct HttpClientPoolConfig {
    /// @brief 目标主机地址
    std::string dst_host = "127.0.0.1";
    /// @brief 目标主机端口
    uint16_t dst_port = 8888;
    /// @brief 初始连接数
    std::size_t init_size = 5;
    /// @brief 最大连接数
    std::size_t max_size = 20;
    /// @brief 连接超时时间（毫秒）
    int connect_timeout_ms = 30000;
    /// @brief 空闲超时时间（秒）
    int idle_timeout_sec = 300;
    /// @brief 每个客户端最大请求数
    std::size_t max_requests_per_client = 100;
};

/// @brief 日志配置
struct LogConfig {
    std::string level = "info";
    std::string dir = "./logs";
    std::string rotation = "daily";
    size_t max_file_size_mb = 100;
    size_t max_files = 5;    
    bool console = true;
    bool json_format = false;
    bool async = true;  // 是否使用异步日志（默认true，调试时可设为false）
};

/// @brief 线程池配置
struct ThreadPoolConfig {
    int min_threads = 2;
    int max_threads = 8;
    int queue_size = 1000;
};

/// @brief ZLM服务器配置
struct ZlmConfig {
    std::string zlm_host = "127.0.0.1";
    int zlm_port = 8888;
    std::string secret = "";
    bool debug_terminal = true;    
};

/// @brief WebSocket服务器配置
struct WebSocketConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 8090;
    int heartbeat_interval = 10;
    int timeout = 30;
};

/// @brief 摄像头数据库配置
struct CameraDbConfig {
    std::string db_path = "./data/camera.db";
    int pool_size = 1;
    // 使用 SQLite3 不需要 host 和 port
    // std::string host = "localhost";
    // int port = 3306;
    // std::string name = "cameras";
    // std::string user = "root";
    // std::string password = "";
    
};

/// @brief 用户数据库配置
struct UserDbConfig {
    std::string db_path = "./data/user.db";
    int pool_size = 1;
    // std::string host = "localhost";
    // int port = 3306;
    // std::string name = "users";
    // std::string user = "root";
    // std::string password = "";
};

/// @brief 应用程序配置
struct AppConfig {
    HttpServerConfig server;
    std::map<std::string, std::vector<HttpClientPoolConfig>> clients;  ///< 多类型客户端池配置（支持多目标）    
    std::map<std::string, LogConfig> logs;
    ZlmConfig zlm;
    WebSocketConfig websocket;
    CameraDbConfig camera_db;       ///< 摄像头数据库配置
    UserDbConfig user_db;         ///< 用户数据库配置
};

class ConfigManager {
public:
    using ConfigChangeCallback = std::function<void(const AppConfig&)>;
    using FieldChangeCallback = std::function<void(const std::string& field, const std::any& old_value, const std::any& new_value)>;

    static ConfigManager& GetInstance();

    bool Load(const std::string& config_path);
    bool Reload();
    bool Save(const std::string& config_path = "");

    const AppConfig& GetConfig() const;
    AppConfig& GetConfig();

    template<typename T>
    T Get(const std::string& key, const T& default_value = T{}) const;

    bool Validate() const;
    std::vector<std::string> GetValidationErrors() const;

    void SetChangeCallback(ConfigChangeCallback callback);
    void CheckAndReload();

    /// @brief 打印配置内容到控制台（用于调试）
    void Dump() const;
    
    // ==================== 动态配置更新功能 ====================
    
    /// @brief 更新整个配置（原子操作）
    /// @param new_config 新配置
    /// @return 成功返回 true
    bool UpdateConfig(const AppConfig& new_config);
    
    /// @brief 获取配置版本号
    /// @return 当前配置版本号
    uint64_t GetConfigVersion() const;
    
    /// @brief 回滚到指定版本
    /// @param version 目标版本号
    /// @return 成功返回 true
    bool RollbackToVersion(uint64_t version);
    
    /// @brief 注册字段变更回调
    /// @param field_path 字段路径，如 "server.port"
    /// @param callback 回调函数
    void OnFieldChange(const std::string& field_path, FieldChangeCallback callback);
    
    /// @brief 移除字段变更回调
    /// @param field_path 字段路径
    void RemoveFieldChangeCallback(const std::string& field_path);

    std::string GetConfigPath() const { return config_path_; }

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void applyDefaults();
    void parseConfig(const YAML::Node& node);
    YAML::Node toYaml() const;
    
    // 辅助函数：获取字段的 any 值
    std::any getFieldAnyValue(const AppConfig& config, const std::string& field_path) const;
    
    // 辅助函数：触发字段变更回调
    void triggerFieldCallbacks(const AppConfig& old_config, const AppConfig& new_config);

    AppConfig config_;
    std::string config_path_;
    std::filesystem::file_time_type last_write_time_;
    mutable std::mutex mutex_;
    ConfigChangeCallback change_callback_;
    
    // 动态配置相关
    std::atomic<uint64_t> config_version_{0};  // 配置版本号
    std::vector<AppConfig> config_history_;     // 配置历史（用于回滚）
    static constexpr size_t MAX_HISTORY_SIZE = 10;  // 最多保存 10 个版本
    std::map<std::string, std::vector<FieldChangeCallback>> field_callbacks_;  // 字段回调
};
