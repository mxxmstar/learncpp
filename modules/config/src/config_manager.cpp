#include "config/common_config.h"
#include <fstream>
#include <sstream>
#include "log/logmanager.h"
ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::load(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        config_path_ = config_path;
        
        if (!std::filesystem::exists(config_path)) {
            applyDefaults();
			LOG_MAIN_WARN_AT("Config file '{}' not found, using default configuration", config_path);
            return true;
        }

        YAML::Node node = YAML::LoadFile(config_path);
        parseConfig(node);
        
        last_write_time_ = std::filesystem::last_write_time(config_path);
        
        return true;
    } catch (const std::exception& e) {
        applyDefaults();
        return false;
    }
}

bool ConfigManager::reload() {
    if (config_path_.empty()) {
        return false;
    }
    return load(config_path_);
}

bool ConfigManager::save(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string path = config_path.empty() ? config_path_ : config_path;
    if (path.empty()) {
        return false;
    }

    try {
        YAML::Node node = toYaml();
        std::ofstream fout(path);
        fout << node;
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

const AppConfig& ConfigManager::getConfig() const {
    return config_;
}

AppConfig& ConfigManager::getConfig() {
    return config_;
}

bool ConfigManager::validate() const {
    return getValidationErrors().empty();
}

std::vector<std::string> ConfigManager::getValidationErrors() const {
    std::vector<std::string> errors;

    if (config_.server.port <= 0 || config_.server.port > 65535) {
        errors.push_back("server.port must be between 1 and 65535");
    }
    if (config_.server.threads <= 0) {
        errors.push_back("server.threads must be positive");
    }

    // 验证 ZLM 客户端池配置
    if (config_.zlm_client.port <= 0 || config_.zlm_client.port > 65535) {
        errors.push_back("clients.zlm.port must be between 1 and 65535");
    }
    if (config_.zlm_client.init_size == 0) {
        errors.push_back("clients.zlm.init_size must be positive");
    }
    if (config_.zlm_client.max_size == 0 || config_.zlm_client.max_size < config_.zlm_client.init_size) {
        errors.push_back("clients.zlm.max_size must be >= init_size");
    }

    // 验证日志配置（支持多个日志实例）
    for (const auto& [log_name, log_config] : config_.logs) {
        if (log_config.max_file_size_mb == 0) {
            errors.push_back("logs." + log_name + ".max_file_size_mb must be positive");
        }
        if (log_config.max_files == 0) {
            errors.push_back("logs." + log_name + ".max_files must be positive");
        }
    }

    // 验证 camera_db 配置
    if (config_.camera_db.port <= 0 || config_.camera_db.port > 65535) {
        errors.push_back("camera_db.port must be between 1 and 65535");
    }
    if (config_.camera_db.name.empty()) {
        errors.push_back("camera_db.name cannot be empty");
    }
    if (config_.camera_db.pool_size <= 0) {
        errors.push_back("camera_db.pool_size must be positive");
    }

    // 验证 user_db 配置
    if (config_.user_db.port <= 0 || config_.user_db.port > 65535) {
        errors.push_back("user_db.port must be between 1 and 65535");
    }
    if (config_.user_db.name.empty()) {
        errors.push_back("user_db.name cannot be empty");
    }
    if (config_.user_db.pool_size <= 0) {
        errors.push_back("user_db.pool_size must be positive");
    }

    if (config_.zlm.zlm_port <= 0 || config_.zlm.zlm_port > 65535) {
        errors.push_back("zlm.zlm_port must be between 1 and 65535");
    }

    if (config_.websocket.port <= 0 || config_.websocket.port > 65535) {
        errors.push_back("websocket.port must be between 1 and 65535");
    }

    return errors;
}

void ConfigManager::setChangeCallback(ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    change_callback_ = std::move(callback);
}

void ConfigManager::checkAndReload() {
    if (config_path_.empty()) {
        return;
    }

    try {
        auto current_write_time = std::filesystem::last_write_time(config_path_);
        if (current_write_time != last_write_time_) {
            AppConfig old_config = config_;
            if (reload()) {
                last_write_time_ = current_write_time;
                if (change_callback_) {
                    change_callback_(config_);
                }
            }
        }
    } catch (...) {
    }
}

void ConfigManager::applyDefaults() {
    config_ = AppConfig{};
}

void ConfigManager::parseConfig(const YAML::Node& node) {
    if (node["server"]) {
        auto& s = config_.server;
        const auto& server = node["server"];
        if (server["host"]) s.host = server["host"].as<std::string>();
        if (server["port"]) s.port = server["port"].as<int>();
        if (server["threads"]) s.threads = server["threads"].as<int>();
    }

    // 解析客户端池配置
    if (node["clients"] && node["clients"]["zlm"]) {
        auto& c = config_.zlm_client;
        const auto& zlm_client = node["clients"]["zlm"];
        if (zlm_client["host"]) c.host = zlm_client["host"].as<std::string>();
        if (zlm_client["port"]) c.port = zlm_client["port"].as<uint16_t>();
        if (zlm_client["init_size"]) c.init_size = zlm_client["init_size"].as<std::size_t>();
        if (zlm_client["max_size"]) c.max_size = zlm_client["max_size"].as<std::size_t>();
        if (zlm_client["connect_timeout_ms"]) c.connect_timeout_ms = zlm_client["connect_timeout_ms"].as<int>();
        if (zlm_client["idle_timeout_sec"]) c.idle_timeout_sec = zlm_client["idle_timeout_sec"].as<int>();
        if (zlm_client["max_requests_per_client"]) c.max_requests_per_client = zlm_client["max_requests_per_client"].as<std::size_t>();
    }

    // 解析多日志配置（支持动态多个日志实例）
    if (node["logs"]) {
        const auto& logs = node["logs"];
        
        // 遍历 logs 下的所有键值对，动态解析每个日志配置
        for (const auto& kv : logs) {
            std::string log_name = kv.first.as<std::string>();
            const auto& log_node = kv.second;
            
            LogConfig log_config;
            if (log_node["level"]) log_config.level = log_node["level"].as<std::string>();
            if (log_node["dir"]) log_config.dir = log_node["dir"].as<std::string>();
            if (log_node["max_file_size_mb"]) log_config.max_file_size_mb = log_node["max_file_size_mb"].as<size_t>();
            if (log_node["max_files"]) log_config.max_files = log_node["max_files"].as<size_t>();
            if (log_node["rotation"]) log_config.rotation = log_node["rotation"].as<std::string>();
            if (log_node["console"]) log_config.console = log_node["console"].as<bool>();
            if (log_node["json_format"]) log_config.json_format = log_node["json_format"].as<bool>();
            
            config_.logs[log_name] = log_config;
        }
    }

    if (node["zlm"]) {
        auto& m = config_.zlm;
        const auto& zlm = node["zlm"];
        if (zlm["zlm_host"]) m.zlm_host = zlm["zlm_host"].as<std::string>();
        if (zlm["zlm_port"]) m.zlm_port = zlm["zlm_port"].as<int>();
        LOG_MAIN_DEBUG_AT("Loaded ZLM port: {}", m.zlm_port);
        if (zlm["secret"]) {
            m.secret = zlm["secret"].as<std::string>();         
            //LOG_MAIN_DEBUG_AT("Loaded ZLM secret: {}", m.secret);
        }
        if (zlm["rtmp_port"]) m.rtmp_port = zlm["rtmp_port"].as<int>();
        if (zlm["rtsp_port"]) m.rtsp_port = zlm["rtsp_port"].as<int>();
    }

    if (node["websocket"]) {
        auto& w = config_.websocket;
        const auto& ws = node["websocket"];
        if (ws["host"]) w.host = ws["host"].as<std::string>();
        if (ws["port"]) w.port = ws["port"].as<uint16_t>();
        if (ws["heartbeat_interval"]) w.heartbeat_interval = ws["heartbeat_interval"].as<int>();
        if (ws["timeout"]) w.timeout = ws["timeout"].as<int>();
    }

    if (node["camera_db"]) {
        auto& c = config_.camera_db;
        const auto& cam = node["camera_db"];
        if (cam["db_path"]) c.db_path = cam["db_path"].as<std::string>();
        if (cam["host"]) c.host = cam["host"].as<std::string>();
        if (cam["port"]) c.port = cam["port"].as<int>();
        if (cam["name"]) c.name = cam["name"].as<std::string>();
        if (cam["user"]) c.user = cam["user"].as<std::string>();
        if (cam["password"]) c.password = cam["password"].as<std::string>();
        if (cam["pool_size"]) c.pool_size = cam["pool_size"].as<int>();
    }

    if (node["user_db"]) {
        auto& u = config_.user_db;
        const auto& user = node["user_db"];
        if (user["db_path"]) u.db_path = user["db_path"].as<std::string>();
        if (user["host"]) u.host = user["host"].as<std::string>();
        if (user["port"]) u.port = user["port"].as<int>();
        if (user["name"]) u.name = user["name"].as<std::string>();
        if (user["user"]) u.user = user["user"].as<std::string>();
        if (user["password"]) u.password = user["password"].as<std::string>();
        if (user["pool_size"]) u.pool_size = user["pool_size"].as<int>();
    }
}

YAML::Node ConfigManager::toYaml() const {
    YAML::Node node;

    node["server"]["host"] = config_.server.host;
    node["server"]["port"] = config_.server.port;
    node["server"]["threads"] = config_.server.threads;

    // 客户端池配置
    node["clients"]["zlm"]["host"] = config_.zlm_client.host;
    node["clients"]["zlm"]["port"] = config_.zlm_client.port;
    node["clients"]["zlm"]["init_size"] = config_.zlm_client.init_size;
    node["clients"]["zlm"]["max_size"] = config_.zlm_client.max_size;
    node["clients"]["zlm"]["connect_timeout_ms"] = config_.zlm_client.connect_timeout_ms;
    node["clients"]["zlm"]["idle_timeout_sec"] = config_.zlm_client.idle_timeout_sec;
    node["clients"]["zlm"]["max_requests_per_client"] = config_.zlm_client.max_requests_per_client;

    // 多日志配置（支持动态多个日志实例）
    for (const auto& [log_name, log_config] : config_.logs) {
        node["logs"][log_name]["level"] = log_config.level;
        node["logs"][log_name]["dir"] = log_config.dir;
        node["logs"][log_name]["max_file_size_mb"] = log_config.max_file_size_mb;
        node["logs"][log_name]["max_files"] = log_config.max_files;
        node["logs"][log_name]["rotation"] = log_config.rotation;
        node["logs"][log_name]["console"] = log_config.console;
        node["logs"][log_name]["json_format"] = log_config.json_format;
    }

    node["zlm"]["zlm_host"] = config_.zlm.zlm_host;
    node["zlm"]["zlm_port"] = config_.zlm.zlm_port;
    node["zlm"]["secret"] = config_.zlm.secret;
    node["zlm"]["rtmp_port"] = config_.zlm.rtmp_port;
    node["zlm"]["rtsp_port"] = config_.zlm.rtsp_port;

    node["websocket"]["host"] = config_.websocket.host;
    node["websocket"]["port"] = config_.websocket.port;
    node["websocket"]["heartbeat_interval"] = config_.websocket.heartbeat_interval;
    node["websocket"]["timeout"] = config_.websocket.timeout;

    node["camera_db"]["db_path"] = config_.camera_db.db_path;
    node["camera_db"]["host"] = config_.camera_db.host;
    node["camera_db"]["port"] = config_.camera_db.port;
    node["camera_db"]["name"] = config_.camera_db.name;
    node["camera_db"]["user"] = config_.camera_db.user;
    node["camera_db"]["password"] = config_.camera_db.password;
    node["camera_db"]["pool_size"] = config_.camera_db.pool_size;

    node["user_db"]["db_path"] = config_.user_db.db_path;
    node["user_db"]["host"] = config_.user_db.host;
    node["user_db"]["port"] = config_.user_db.port;
    node["user_db"]["name"] = config_.user_db.name;
    node["user_db"]["user"] = config_.user_db.user;
    node["user_db"]["password"] = config_.user_db.password;
    node["user_db"]["pool_size"] = config_.user_db.pool_size;

    return node;
}

namespace config_utils {
    spdlog::level::level_enum parseLogLevel(const std::string& level) {
        if (level == "trace") return spdlog::level::trace;
        if (level == "debug") return spdlog::level::debug;
        if (level == "info") return spdlog::level::info;
        if (level == "warn" || level == "warning") return spdlog::level::warn;
        if (level == "error") return spdlog::level::err;
        if (level == "critical") return spdlog::level::critical;
        if (level == "off") return spdlog::level::off;
        return spdlog::level::info;
    }

    std::string logLevelToString(spdlog::level::level_enum level) {
        switch (level) {
            case spdlog::level::trace: return "trace";
            case spdlog::level::debug: return "debug";
            case spdlog::level::info: return "info";
            case spdlog::level::warn: return "warn";
            case spdlog::level::err: return "error";
            case spdlog::level::critical: return "critical";
            case spdlog::level::off: return "off";
            default: return "info";
        }
    }
}
