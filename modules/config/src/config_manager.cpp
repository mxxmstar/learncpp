#include "config/common_config.h"
#include <fstream>
#include <sstream>
#include <iostream>

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
            std::cerr << "[Config] Config file '" << config_path << "' not found, using default configuration" << std::endl;
            return true;
        }

        YAML::Node node = YAML::LoadFile(config_path);
        parseConfig(node);
        
        last_write_time_ = std::filesystem::last_write_time(config_path);
        
        return true;
    } catch (const std::exception& e) {
        applyDefaults();
        std::cerr << "[Config] Failed to load config: " << e.what() << std::endl;
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

    // 验证 HTTP 服务器配置
    if (config_.server.port <= 0 || config_.server.port > 65535) {
        errors.push_back("server.port must be between 1 and 65535");
    }

    // 验证 ZLM 客户端池配置
    if (config_.zlm_client.dst_port <= 0 || config_.zlm_client.dst_port > 65535) {
        errors.push_back("clients.zlm.dst_port must be between 1 and 65535");
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
    if (config_.camera_db.pool_size <= 0) {
        errors.push_back("camera_db.pool_size must be positive");
    }

    // 验证 user_db 配置
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
    }

    // 解析客户端池配置
    if (node["clients"] && node["clients"]["zlm"]) {
        auto& c = config_.zlm_client;
        const auto& zlm_client = node["clients"]["zlm"];
        if (zlm_client["dst_host"]) c.dst_host = zlm_client["dst_host"].as<std::string>();
        if (zlm_client["dst_port"]) c.dst_port = zlm_client["dst_port"].as<uint16_t>();
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
        std::cout << "[Config] Loaded ZLM port: " << m.zlm_port << std::endl;
        if (zlm["secret"]) {
            m.secret = zlm["secret"].as<std::string>();         
            // std::cout << "[Config] Loaded ZLM secret: " << m.secret << std::endl;
        }
        if (zlm["debug_terminal"]) m.debug_terminal = zlm["debug_terminal"].as<bool>();
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
        if (cam["pool_size"]) c.pool_size = cam["pool_size"].as<int>();
    }

    if (node["user_db"]) {
        auto& u = config_.user_db;
        const auto& user = node["user_db"];
        if (user["db_path"]) u.db_path = user["db_path"].as<std::string>();
        if (user["pool_size"]) u.pool_size = user["pool_size"].as<int>();
    }
}

YAML::Node ConfigManager::toYaml() const {
    YAML::Node node;

    node["server"]["host"] = config_.server.host;
    node["server"]["port"] = config_.server.port;

    // 客户端池配置
    node["clients"]["zlm"]["dst_host"] = config_.zlm_client.dst_host;
    node["clients"]["zlm"]["dst_port"] = config_.zlm_client.dst_port;
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
    node["zlm"]["debug_terminal"] = config_.zlm.debug_terminal;

    node["websocket"]["host"] = config_.websocket.host;
    node["websocket"]["port"] = config_.websocket.port;
    node["websocket"]["heartbeat_interval"] = config_.websocket.heartbeat_interval;
    node["websocket"]["timeout"] = config_.websocket.timeout;

    node["camera_db"]["db_path"] = config_.camera_db.db_path;
    node["camera_db"]["pool_size"] = config_.camera_db.pool_size;

    node["user_db"]["db_path"] = config_.user_db.db_path;
    node["user_db"]["pool_size"] = config_.user_db.pool_size;

    return node;
}

void ConfigManager::dump() const {
    std::cout << "\n========== AppConfig Dump ==========" << std::endl;
    
    // HTTP Server
    std::cout << "\n[HTTP Server]" << std::endl;
    std::cout << "  host: " << config_.server.host << std::endl;
    std::cout << "  port: " << config_.server.port << std::endl;
    
    // ZLM Client Pool
    std::cout << "\n[ZLM Client Pool]" << std::endl;
    std::cout << "  dst_host: " << config_.zlm_client.dst_host << std::endl;
    std::cout << "  dst_port: " << config_.zlm_client.dst_port << std::endl;
    std::cout << "  init_size: " << config_.zlm_client.init_size << std::endl;
    std::cout << "  max_size: " << config_.zlm_client.max_size << std::endl;
    std::cout << "  connect_timeout_ms: " << config_.zlm_client.connect_timeout_ms << std::endl;
    std::cout << "  idle_timeout_sec: " << config_.zlm_client.idle_timeout_sec << std::endl;
    std::cout << "  max_requests_per_client: " << config_.zlm_client.max_requests_per_client << std::endl;
    
    // Logs
    std::cout << "\n[Logs]" << std::endl;
    for (const auto& [name, log_cfg] : config_.logs) {
        std::cout << "  [" << name << "]" << std::endl;
        std::cout << "    level: " << log_cfg.level << std::endl;
        std::cout << "    dir: " << log_cfg.dir << std::endl;
        std::cout << "    rotation: " << log_cfg.rotation << std::endl;
        std::cout << "    max_file_size_mb: " << log_cfg.max_file_size_mb << std::endl;
        std::cout << "    max_files: " << log_cfg.max_files << std::endl;
        std::cout << "    console: " << (log_cfg.console ? "true" : "false") << std::endl;
        std::cout << "    json_format: " << (log_cfg.json_format ? "true" : "false") << std::endl;
    }
    
    // ZLM Server
    std::cout << "\n[ZLM Server]" << std::endl;
    std::cout << "  zlm_host: " << config_.zlm.zlm_host << std::endl;
    std::cout << "  zlm_port: " << config_.zlm.zlm_port << std::endl;
    std::cout << "  secret: " << (config_.zlm.secret.empty() ? "(empty)" : "***") << std::endl;
    std::cout << "  debug_terminal: " << (config_.zlm.debug_terminal ? "true" : "false") << std::endl;
    
    // WebSocket
    std::cout << "\n[WebSocket]" << std::endl;
    std::cout << "  host: " << config_.websocket.host << std::endl;
    std::cout << "  port: " << config_.websocket.port << std::endl;
    std::cout << "  heartbeat_interval: " << config_.websocket.heartbeat_interval << std::endl;
    std::cout << "  timeout: " << config_.websocket.timeout << std::endl;
    
    // Camera DB
    std::cout << "\n[Camera DB]" << std::endl;
    std::cout << "  db_path: " << config_.camera_db.db_path << std::endl;
    std::cout << "  pool_size: " << config_.camera_db.pool_size << std::endl;
    
    // User DB
    std::cout << "\n[User DB]" << std::endl;
    std::cout << "  db_path: " << config_.user_db.db_path << std::endl;
    std::cout << "  pool_size: " << config_.user_db.pool_size << std::endl;
    
    std::cout << "\n======================================\n" << std::endl;
}

// ==================== 动态配置更新功能实现 ====================

bool ConfigManager::updateConfig(const AppConfig& new_config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 1. 验证新配置
        AppConfig temp_config = config_;  // 保存旧配置
        config_ = new_config;
        
        auto errors = getValidationErrors();
        if (!errors.empty()) {
            config_ = temp_config;  // 恢复旧配置
            std::cerr << "[Config] Validation failed:" << std::endl;
            for (const auto& err : errors) {
                std::cerr << "  - " << err << std::endl;
            }
            return false;
        }
        
        // 2. 保存旧配置到历史
        config_history_.push_back(temp_config);
        if (config_history_.size() > MAX_HISTORY_SIZE) {
            config_history_.erase(config_history_.begin());  // 移除最旧的
        }
        
        // 3. 增加版本号
        config_version_.fetch_add(1);
        
        // 4. 触发字段变更回调
        triggerFieldCallbacks(temp_config, new_config);
        
        // 5. 触发全局配置变更回调
        if (change_callback_) {
            change_callback_(config_);
        }
        
        std::cout << "[Config] Configuration updated successfully (version: " 
                  << config_version_.load() << ")" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[Config] Failed to update config: " << e.what() << std::endl;
        return false;
    }
}

uint64_t ConfigManager::getConfigVersion() const {
    return config_version_.load();
}

bool ConfigManager::rollbackToVersion(uint64_t version) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 计算需要回滚的索引
    // 当前版本是 config_version_，历史中最后一个是 version-1
    uint64_t current_version = config_version_.load();
    if (version >= current_version) {
        std::cerr << "[Config] Cannot rollback to current or future version" << std::endl;
        return false;
    }
    
    size_t history_index = config_history_.size() - (current_version - version);
    if (history_index >= config_history_.size()) {
        std::cerr << "[Config] Version " << version << " not found in history" << std::endl;
        return false;
    }
    
    try {
        AppConfig old_config = config_;
        config_ = config_history_[history_index];
        
        // 更新版本号
        config_version_.store(version);
        
        // 触发回调
        triggerFieldCallbacks(old_config, config_);
        if (change_callback_) {
            change_callback_(config_);
        }
        
        std::cout << "[Config] Rolled back to version " << version << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[Config] Failed to rollback: " << e.what() << std::endl;
        return false;
    }
}

void ConfigManager::onFieldChange(const std::string& field_path, FieldChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    field_callbacks_[field_path].push_back(std::move(callback));
}

void ConfigManager::removeFieldChangeCallback(const std::string& field_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    field_callbacks_.erase(field_path);
}

std::any ConfigManager::getFieldAnyValue(const AppConfig& config, const std::string& field_path) const {
    // 解析字段路径，如 "server.port"
    if (field_path == "server.host") {
        return std::any(config.server.host);
    } else if (field_path == "server.port") {
        return std::any(config.server.port);
    } else if (field_path == "zlm_client.dst_host") {
        return std::any(config.zlm_client.dst_host);
    } else if (field_path == "zlm_client.dst_port") {
        return std::any(config.zlm_client.dst_port);
    } else if (field_path == "zlm.zlm_host") {
        return std::any(config.zlm.zlm_host);
    } else if (field_path == "zlm.zlm_port") {
        return std::any(config.zlm.zlm_port);
    } else if (field_path == "websocket.host") {
        return std::any(config.websocket.host);
    } else if (field_path == "websocket.port") {
        return std::any(config.websocket.port);
    } else if (field_path == "camera_db.db_path") {
        return std::any(config.camera_db.db_path);
    } else if (field_path == "user_db.db_path") {
        return std::any(config.user_db.db_path);
    }
    
    return std::any();  // 返回空的 any
}

void ConfigManager::triggerFieldCallbacks(const AppConfig& old_config, const AppConfig& new_config) {
    for (const auto& [field_path, callbacks] : field_callbacks_) {
        auto old_value = getFieldAnyValue(old_config, field_path);
        auto new_value = getFieldAnyValue(new_config, field_path);
        
        // 如果值发生变化，触发回调
        if (old_value.has_value() && new_value.has_value()) {
            // 简单比较（对于基本类型有效）
            bool changed = false;
            
            // 尝试比较字符串
            if (old_value.type() == typeid(std::string) && new_value.type() == typeid(std::string)) {
                changed = (std::any_cast<std::string>(old_value) != std::any_cast<std::string>(new_value));
            }
            // 尝试比较 int
            else if (old_value.type() == typeid(int) && new_value.type() == typeid(int)) {
                changed = (std::any_cast<int>(old_value) != std::any_cast<int>(new_value));
            }
            // 尝试比较 uint16_t
            else if (old_value.type() == typeid(uint16_t) && new_value.type() == typeid(uint16_t)) {
                changed = (std::any_cast<uint16_t>(old_value) != std::any_cast<uint16_t>(new_value));
            }
            // 其他类型默认认为变化了
            else {
                changed = true;
            }
            
            if (changed) {
                for (const auto& callback : callbacks) {
                    try {
                        callback(field_path, old_value, new_value);
                    } catch (const std::exception& e) {
                        std::cerr << "[Config] Field callback exception: " << e.what() << std::endl;
                    }
                }
            }
        }
    }
}
