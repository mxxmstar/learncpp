#include "common/config/common_config.h"
#include "common/log/logmanager.h"
#include <fstream>
#include <sstream>

ConfigManager& ConfigManager::GetInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::Load(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        config_path_ = config_path;
        
        if (!std::filesystem::exists(config_path)) {
            applyDefaults();
            LOG_MAIN_WARN_AT("[Config] Config file '{}' not found, using default configuration", config_path);
            return true;
        }

        LOG_MAIN_INFO_AT("[Config] Loading config from: {}", config_path);
        YAML::Node node = YAML::LoadFile(config_path);
        
        // 调试：检查 logs 节点是否存在
        if (node["logs"]) {            
            int log_count = 0;
            for (const auto& kv : node["logs"]) {
                log_count++;
                LOG_MAIN_INFO_AT("[Config]   - Log entry: {}", kv.first.as<std::string>());
            }            
        } else {
            LOG_MAIN_WARN_AT("[Config] No 'logs' node found in YAML!");
        }
        
        parseConfig(node);
        
        LOG_MAIN_INFO_AT("[Config] After parseConfig, logs count: {}", config_.logs.size());
        
        last_write_time_ = std::filesystem::last_write_time(config_path);
        
        return true;
    } catch (const std::exception& e) {
        applyDefaults();
        LOG_MAIN_ERROR_AT("[Config] Failed to load config: {}", e.what());
        return false;
    }
}

bool ConfigManager::Reload() {
    if (config_path_.empty()) {
        return false;
    }
    return Load(config_path_);
}

bool ConfigManager::Save(const std::string& config_path) {
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

const AppConfig& ConfigManager::GetConfig() const {
    return config_;
}

AppConfig& ConfigManager::GetConfig() {
    return config_;
}

bool ConfigManager::Validate() const {
    return GetValidationErrors().empty();
}

std::vector<std::string> ConfigManager::GetValidationErrors() const {
    std::vector<std::string> errors;

    // 验证 HTTP 服务器配置
    if (config_.server.port <= 0 || config_.server.port > 65535) {
        errors.push_back("server.port must be between 1 and 65535");
    }

    // 验证客户端池配置
    for (const auto& [client_type, configs] : config_.clients) {
        if (configs.empty()) {
            errors.push_back("clients." + client_type + " must have at least one configuration");
        } else {
            for (size_t i = 0; i < configs.size(); ++i) {
                const auto& client = configs[i];
                if (client.dst_port <= 0 || client.dst_port > 65535) {
                    errors.push_back("clients." + client_type + "[" + std::to_string(i) + "].dst_port must be between 1 and 65535");
                }
                if (client.init_size == 0) {
                    errors.push_back("clients." + client_type + "[" + std::to_string(i) + "].init_size must be positive");
                }
                if (client.max_size == 0 || client.max_size < client.init_size) {
                    errors.push_back("clients." + client_type + "[" + std::to_string(i) + "].max_size must be >= init_size");
                }
            }
        }
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

void ConfigManager::SetChangeCallback(ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    change_callback_ = std::move(callback);
}

void ConfigManager::CheckAndReload() {
    if (config_path_.empty()) {
        return;
    }

    try {
        auto current_write_time = std::filesystem::last_write_time(config_path_);
        if (current_write_time != last_write_time_) {
            AppConfig old_config = config_;
            if (Reload()) {
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

    // 解析客户端池配置（支持多类型、多目标）
    if (node["clients"]) {
        const auto& clients_node = node["clients"];
        
        // 遍历所有客户端类型（zlm, api_server, database, ...）
        for (const auto& kv : clients_node) {
            std::string client_type = kv.first.as<std::string>();
            const auto& type_node = kv.second;
            
            std::vector<HttpClientPoolConfig> configs;
            
            // 检查是数组还是单个配置
            if (type_node.IsSequence()) {
                // 新格式：数组
                for (const auto& client_node : type_node) {
                    HttpClientPoolConfig config;
                    if (client_node["dst_host"]) config.dst_host = client_node["dst_host"].as<std::string>();
                    if (client_node["dst_port"]) config.dst_port = client_node["dst_port"].as<uint16_t>();
                    if (client_node["init_size"]) config.init_size = client_node["init_size"].as<std::size_t>();
                    if (client_node["max_size"]) config.max_size = client_node["max_size"].as<std::size_t>();
                    if (client_node["connect_timeout_ms"]) config.connect_timeout_ms = client_node["connect_timeout_ms"].as<int>();
                    if (client_node["idle_timeout_sec"]) config.idle_timeout_sec = client_node["idle_timeout_sec"].as<int>();
                    if (client_node["max_requests_per_client"]) config.max_requests_per_client = client_node["max_requests_per_client"].as<std::size_t>();
                    configs.push_back(config);
                }
            } else {
                // 旧格式：单个配置（向后兼容）
                HttpClientPoolConfig config;
                if (type_node["dst_host"]) config.dst_host = type_node["dst_host"].as<std::string>();
                if (type_node["dst_port"]) config.dst_port = type_node["dst_port"].as<uint16_t>();
                if (type_node["init_size"]) config.init_size = type_node["init_size"].as<std::size_t>();
                if (type_node["max_size"]) config.max_size = type_node["max_size"].as<std::size_t>();
                if (type_node["connect_timeout_ms"]) config.connect_timeout_ms = type_node["connect_timeout_ms"].as<int>();
                if (type_node["idle_timeout_sec"]) config.idle_timeout_sec = type_node["idle_timeout_sec"].as<int>();
                if (type_node["max_requests_per_client"]) config.max_requests_per_client = type_node["max_requests_per_client"].as<std::size_t>();
                configs.push_back(config);
            }
            
            config_.clients[client_type] = configs;
        }
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
            if (log_node["async"]) log_config.async = log_node["async"].as<bool>();
            
            config_.logs[log_name] = log_config;
        }
    }

    if (node["zlm"]) {
        auto& m = config_.zlm;
        const auto& zlm = node["zlm"];
        if (zlm["zlm_host"]) m.zlm_host = zlm["zlm_host"].as<std::string>();
        if (zlm["zlm_port"]) m.zlm_port = zlm["zlm_port"].as<int>();
        LOG_MAIN_INFO_AT("[Config] Loaded ZLM port: {}", m.zlm_port);
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

    // 客户端池配置（支持多类型、多目标）
    for (const auto& [client_type, configs] : config_.clients) {
        if (configs.empty()) {
            continue;
        }
        
        if (configs.size() == 1) {
            // 单个配置：保存为对象格式（向后兼容）
            const auto& client = configs[0];
            node["clients"][client_type]["dst_host"] = client.dst_host;
            node["clients"][client_type]["dst_port"] = client.dst_port;
            node["clients"][client_type]["init_size"] = client.init_size;
            node["clients"][client_type]["max_size"] = client.max_size;
            node["clients"][client_type]["connect_timeout_ms"] = client.connect_timeout_ms;
            node["clients"][client_type]["idle_timeout_sec"] = client.idle_timeout_sec;
            node["clients"][client_type]["max_requests_per_client"] = client.max_requests_per_client;
        } else {
            // 多个配置：保存为数组格式
            YAML::Node clients_array(YAML::NodeType::Sequence);
            for (const auto& client : configs) {
                YAML::Node client_node;
                client_node["dst_host"] = client.dst_host;
                client_node["dst_port"] = client.dst_port;
                client_node["init_size"] = client.init_size;
                client_node["max_size"] = client.max_size;
                client_node["connect_timeout_ms"] = client.connect_timeout_ms;
                client_node["idle_timeout_sec"] = client.idle_timeout_sec;
                client_node["max_requests_per_client"] = client.max_requests_per_client;
                clients_array.push_back(client_node);
            }
            node["clients"][client_type] = clients_array;
        }
    }

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

void ConfigManager::Dump() const {
    LOG_MAIN_INFO_AT("");
    LOG_MAIN_INFO_AT("========== AppConfig Dump ==========");
    
    // HTTP Server
    LOG_MAIN_INFO_AT("");
    LOG_MAIN_INFO_AT("[HTTP Server]");
    LOG_MAIN_INFO_AT("  host: {}", config_.server.host);
    LOG_MAIN_INFO_AT("  port: {}", config_.server.port);
    
    // Client Pools
    LOG_MAIN_INFO_AT("");
    LOG_MAIN_INFO_AT("[Client Pools] (types: {})", config_.clients.size());
    if (config_.clients.empty()) {
        LOG_MAIN_WARN_AT("  [WARNING] No client configurations found!");
    } else {
        for (const auto& [client_type, configs] : config_.clients) {
            LOG_MAIN_INFO_AT("  [Type: {}] (count: {})", client_type, configs.size());
            for (size_t i = 0; i < configs.size(); ++i) {
                const auto& client = configs[i];
                LOG_MAIN_INFO_AT("    [Pool {}]", i);
                LOG_MAIN_INFO_AT("      dst_host: {}", client.dst_host);
                LOG_MAIN_INFO_AT("      dst_port: {}", client.dst_port);
                LOG_MAIN_INFO_AT("      init_size: {}", client.init_size);
                LOG_MAIN_INFO_AT("      max_size: {}", client.max_size);
                LOG_MAIN_INFO_AT("      connect_timeout_ms: {}", client.connect_timeout_ms);
                LOG_MAIN_INFO_AT("      idle_timeout_sec: {}", client.idle_timeout_sec);
                LOG_MAIN_INFO_AT("      max_requests_per_client: {}", client.max_requests_per_client);
            }
        }
    }
    
    // Logs
    LOG_MAIN_INFO_AT("");
    LOG_MAIN_INFO_AT("[Logs] (count: {})", config_.logs.size());
    if (config_.logs.empty()) {
        LOG_MAIN_WARN_AT("  [WARNING] No log configurations found!");
        LOG_MAIN_WARN_AT("  Config file: {}", config_path_);
        LOG_MAIN_WARN_AT("  This may indicate the config file was not loaded correctly.");
    }
    for (const auto& [name, log_cfg] : config_.logs) {
        LOG_MAIN_INFO_AT("  [{}]", name);
        LOG_MAIN_INFO_AT("    level: {}", log_cfg.level);
        LOG_MAIN_INFO_AT("    dir: {}", log_cfg.dir);
        LOG_MAIN_INFO_AT("    rotation: {}", log_cfg.rotation);
        LOG_MAIN_INFO_AT("    max_file_size_mb: {}", log_cfg.max_file_size_mb);
        LOG_MAIN_INFO_AT("    max_files: {}", log_cfg.max_files);
        LOG_MAIN_INFO_AT("    console: {}", log_cfg.console ? "true" : "false");
        LOG_MAIN_INFO_AT("    json_format: {}", log_cfg.json_format ? "true" : "false");
    }
    
    // ZLM Server
    LOG_MAIN_INFO_AT("");
    LOG_MAIN_INFO_AT("[ZLM Server]");
    LOG_MAIN_INFO_AT("  zlm_host: {}", config_.zlm.zlm_host);
    LOG_MAIN_INFO_AT("  zlm_port: {}", config_.zlm.zlm_port);
    LOG_MAIN_INFO_AT("  secret: {}", config_.zlm.secret.empty() ? "(empty)" : "***");
    LOG_MAIN_INFO_AT("  debug_terminal: {}", config_.zlm.debug_terminal ? "true" : "false");
    
    // WebSocket
    LOG_MAIN_INFO_AT("");
    LOG_MAIN_INFO_AT("[WebSocket]");
    LOG_MAIN_INFO_AT("  host: {}", config_.websocket.host);
    LOG_MAIN_INFO_AT("  port: {}", config_.websocket.port);
    LOG_MAIN_INFO_AT("  heartbeat_interval: {}", config_.websocket.heartbeat_interval);
    LOG_MAIN_INFO_AT("  timeout: {}", config_.websocket.timeout);
    
    // Camera DB
    LOG_MAIN_INFO_AT("");
    LOG_MAIN_INFO_AT("[Camera DB]");
    LOG_MAIN_INFO_AT("  db_path: {}", config_.camera_db.db_path);
    LOG_MAIN_INFO_AT("  pool_size: {}", config_.camera_db.pool_size);
    
    // User DB
    LOG_MAIN_INFO_AT("");
    LOG_MAIN_INFO_AT("[User DB]");
    LOG_MAIN_INFO_AT("  db_path: {}", config_.user_db.db_path);
    LOG_MAIN_INFO_AT("  pool_size: {}", config_.user_db.pool_size);
    
    LOG_MAIN_INFO_AT("");
    LOG_MAIN_INFO_AT("======================================");
    LOG_MAIN_INFO_AT("");
}

// ==================== 动态配置更新功能实现 ====================

bool ConfigManager::UpdateConfig(const AppConfig& new_config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 1. 验证新配置
        AppConfig temp_config = config_;  // 保存旧配置
        config_ = new_config;
        
        auto errors = GetValidationErrors();
        if (!errors.empty()) {
            config_ = temp_config;  // 恢复旧配置
            LOG_MAIN_ERROR_AT("[Config] Validation failed:");
            for (const auto& err : errors) {
                LOG_MAIN_ERROR_AT("  - {}", err);
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
        
        LOG_MAIN_INFO_AT("[Config] Configuration updated successfully (version: {})", config_version_.load());
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("[Config] Failed to update config: {}", e.what());
        return false;
    }
}

uint64_t ConfigManager::GetConfigVersion() const {
    return config_version_.load();
}

bool ConfigManager::RollbackToVersion(uint64_t version) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 计算需要回滚的索引
    // 当前版本是 config_version_，历史中最后一个是 version-1
    uint64_t current_version = config_version_.load();
    if (version >= current_version) {
        LOG_MAIN_ERROR_AT("[Config] Cannot rollback to current or future version");
        return false;
    }
    
    size_t history_index = config_history_.size() - (current_version - version);
    if (history_index >= config_history_.size()) {
        LOG_MAIN_ERROR_AT("[Config] Version {} not found in history", version);
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
        
        LOG_MAIN_INFO_AT("[Config] Rolled back to version {}", version);
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("[Config] Failed to rollback: {}", e.what());
        return false;
    }
}

void ConfigManager::OnFieldChange(const std::string& field_path, FieldChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    field_callbacks_[field_path].push_back(std::move(callback));
}

void ConfigManager::RemoveFieldChangeCallback(const std::string& field_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    field_callbacks_.erase(field_path);
}

std::any ConfigManager::getFieldAnyValue(const AppConfig& config, const std::string& field_path) const {
    // 解析字段路径，如 "server.port"
    if (field_path == "server.host") {
        return std::any(config.server.host);
    } else if (field_path == "server.port") {
        return std::any(config.server.port);
    } else if (field_path == "clients.zlm.dst_host") {
        // 获取第一个 ZLM 客户端的 dst_host
        auto it = config.clients.find("zlm");
        if (it != config.clients.end() && !it->second.empty()) {
            return std::any(it->second[0].dst_host);
        }
        return std::any(std::string{});
    } else if (field_path == "clients.zlm.dst_port") {
        // 获取第一个 ZLM 客户端的 dst_port
        auto it = config.clients.find("zlm");
        if (it != config.clients.end() && !it->second.empty()) {
            return std::any(it->second[0].dst_port);
        }
        return std::any(uint16_t{0});
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
                        LOG_MAIN_ERROR_AT("[Config] Field callback exception: {}", e.what());
                    }
                }
            }
        }
    }
}
