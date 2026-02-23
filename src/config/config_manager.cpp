#include "config/common_config.h"
#include <fstream>
#include <sstream>

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

    if (config_.log.max_file_size_mb == 0) {
        errors.push_back("log.max_file_size_mb must be positive");
    }
    if (config_.log.max_files == 0) {
        errors.push_back("log.max_files must be positive");
    }

    if (config_.database.enabled) {
        if (config_.database.host.empty()) {
            errors.push_back("database.host cannot be empty when database is enabled");
        }
        if (config_.database.port <= 0 || config_.database.port > 65535) {
            errors.push_back("database.port must be between 1 and 65535");
        }
        if (config_.database.name.empty()) {
            errors.push_back("database.name cannot be empty when database is enabled");
        }
        if (config_.database.pool_size <= 0) {
            errors.push_back("database.pool_size must be positive");
        }
    }

    if (config_.thread_pool.min_threads <= 0) {
        errors.push_back("thread_pool.min_threads must be positive");
    }
    if (config_.thread_pool.max_threads < config_.thread_pool.min_threads) {
        errors.push_back("thread_pool.max_threads must be >= min_threads");
    }
    if (config_.thread_pool.queue_size <= 0) {
        errors.push_back("thread_pool.queue_size must be positive");
    }

    if (config_.media.zlm_port <= 0 || config_.media.zlm_port > 65535) {
        errors.push_back("media.zlm_port must be between 1 and 65535");
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

    if (node["log"]) {
        auto& l = config_.log;
        const auto& log = node["log"];
        if (log["level"]) l.level = log["level"].as<std::string>();
        if (log["dir"]) l.dir = log["dir"].as<std::string>();
        if (log["max_file_size_mb"]) l.max_file_size_mb = log["max_file_size_mb"].as<size_t>();
        if (log["max_files"]) l.max_files = log["max_files"].as<size_t>();
        if (log["rotation"]) l.rotation = log["rotation"].as<std::string>();
        if (log["console"]) l.console = log["console"].as<bool>();
        if (log["json_format"]) l.json_format = log["json_format"].as<bool>();
    }

    if (node["database"]) {
        auto& d = config_.database;
        const auto& db = node["database"];
        if (db["enabled"]) d.enabled = db["enabled"].as<bool>();
        if (db["host"]) d.host = db["host"].as<std::string>();
        if (db["port"]) d.port = db["port"].as<int>();
        if (db["name"]) d.name = db["name"].as<std::string>();
        if (db["user"]) d.user = db["user"].as<std::string>();
        if (db["password"]) d.password = db["password"].as<std::string>();
        if (db["pool_size"]) d.pool_size = db["pool_size"].as<int>();
    }

    if (node["thread_pool"]) {
        auto& t = config_.thread_pool;
        const auto& tp = node["thread_pool"];
        if (tp["min_threads"]) t.min_threads = tp["min_threads"].as<int>();
        if (tp["max_threads"]) t.max_threads = tp["max_threads"].as<int>();
        if (tp["queue_size"]) t.queue_size = tp["queue_size"].as<int>();
    }

    if (node["media"]) {
        auto& m = config_.media;
        const auto& media = node["media"];
        if (media["zlm_host"]) m.zlm_host = media["zlm_host"].as<std::string>();
        if (media["zlm_port"]) m.zlm_port = media["zlm_port"].as<int>();
        if (media["secret"]) m.secret = media["secret"].as<std::string>();
        if (media["rtmp_port"]) m.rtmp_port = media["rtmp_port"].as<int>();
        if (media["rtsp_port"]) m.rtsp_port = media["rtsp_port"].as<int>();
    }
}

YAML::Node ConfigManager::toYaml() const {
    YAML::Node node;

    node["server"]["host"] = config_.server.host;
    node["server"]["port"] = config_.server.port;
    node["server"]["threads"] = config_.server.threads;

    node["log"]["level"] = config_.log.level;
    node["log"]["dir"] = config_.log.dir;
    node["log"]["max_file_size_mb"] = config_.log.max_file_size_mb;
    node["log"]["max_files"] = config_.log.max_files;
    node["log"]["rotation"] = config_.log.rotation;
    node["log"]["console"] = config_.log.console;
    node["log"]["json_format"] = config_.log.json_format;

    node["database"]["enabled"] = config_.database.enabled;
    node["database"]["host"] = config_.database.host;
    node["database"]["port"] = config_.database.port;
    node["database"]["name"] = config_.database.name;
    node["database"]["user"] = config_.database.user;
    node["database"]["password"] = config_.database.password;
    node["database"]["pool_size"] = config_.database.pool_size;

    node["thread_pool"]["min_threads"] = config_.thread_pool.min_threads;
    node["thread_pool"]["max_threads"] = config_.thread_pool.max_threads;
    node["thread_pool"]["queue_size"] = config_.thread_pool.queue_size;

    node["media"]["zlm_host"] = config_.media.zlm_host;
    node["media"]["zlm_port"] = config_.media.zlm_port;
    node["media"]["secret"] = config_.media.secret;
    node["media"]["rtmp_port"] = config_.media.rtmp_port;
    node["media"]["rtsp_port"] = config_.media.rtsp_port;

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
