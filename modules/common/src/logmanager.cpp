#include "common/log/logmanager.h"
#include "common/config/common_config.h"  // 需要 LogConfig 的完整定义
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

LogManager& LogManager::getInstance() {
    static LogManager instance;
    return instance;
}

void LogManager::Init(const std::string& base_dir, int async_threads) {
    if (initialized_) {
        return;
    }

    // 初始化线程池
    spdlog::init_thread_pool(8192, async_threads);

    // 初始化日志目录
    log_dir_ = base_dir;
    if (log_dir_.back() != '/') {
        log_dir_ += '/';
    }

    // 创建简单的日志器（第一阶段）
    auto main_config = LoggerConfig("main");    
    auto error_config = LoggerConfig("error", spdlog::level::err);
#if DEBUG_MODE
    main_config.level = spdlog::level::trace;    // 调试模式下，日志级别为 trace
    main_config.async = false;  // 调试模式下，使用同步日志，确保日志按顺序输出，便于调试
    error_config.async = false;  // 调试模式下，使用同步日志，确保错误日志按顺序输出，便于调试
#else    
    main_config.level = spdlog::level::info;    // 非调试模式下，日志级别为 info
    main_config.async = true;  // 非调试模式下，使用异步日志，提高性能    
    error_config.async = true;  // 非调试模式下，使用异步日志，提高性能    
#endif
    loggers_["main"] = std::make_shared<Logger>(main_config);
    loggers_["error"] = std::make_shared<Logger>(error_config);
    
    initialized_ = true;
}

void LogManager::ReloadFromConfig(const LoggerConfig& config) {
    if (!initialized_) {
        // 如果还未初始化，先调用 Init
		// TODO: 这里可以考虑是否允许直接使用 LogConfig 来初始化日志系统，添加一个专门的 InitFromConfig 方法
        Init();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 重新配置 logger
    auto it = loggers_.find(config.name);
    if (it == loggers_.end()) {
        // 如果 logger 不存在，创建它
        auto new_logger = std::make_shared<Logger>(config);
        loggers_[config.name] = new_logger;
        LOG_MAIN_INFO_AT("Created new logger: {}", config.name);
        return;
    }
    
    bool needs_rebuild = false;
    
    // 1. 热更新日志级别（立即生效）
    if (it->second->GetLevel() != config.level) {
        it->second->SetLevel(config.level);
        LOG_MAIN_INFO_AT("Logger '{}' level updated: {} -> {}", 
            config.name, 
            spdlog::level::to_string_view(it->second->GetLevel()),
            spdlog::level::to_string_view(config.level));
    }
    
    // 2. 检查是否需要重建 sink（文件路径、滚动策略等变更）
    if (it->second->GetRotationPolicy() != config.policy ||
        it->second->GetMaxFileSize() != config.max_file_size_mb ||
        it->second->GetMaxFiles() != config.max_files ||
        it->second->GetLogDir() != config.log_dir) {
        needs_rebuild = true;
    }

    if (needs_rebuild) {
        LOG_MAIN_WARN_AT("Logger '{}' requires rebuild due to configuration change", 
            config.name);
        // 刷新旧的 logger
        it->second->Flush();
        
        // 创建新的 logger 替换旧的
        auto new_logger = std::make_shared<Logger>(config);
        loggers_[config.name] = new_logger;
        
        LOG_MAIN_INFO_AT("Logger '{}' rebuilt with new configuration", config.name);
        return;
    }
    
    LOG_MAIN_INFO_AT("Logger '{}' configuration reloaded", config.name);
}

void LogManager::ReloadFromConfigs(const std::map<std::string, LogConfig>& configs) {
    if (!initialized_) {
        Init();
    }
    
    LOG_MAIN_INFO_AT("[LogManager] Reloading {} logger configurations...", configs.size());
    
    for (const auto& [name, log_cfg] : configs) {
        // 转换为 LoggerConfig
        LoggerConfig logger_cfg = ConvertToLoggerConfig(log_cfg, name);
        
        // 调用单个配置的 ReloadFromConfig
        ReloadFromConfig(logger_cfg);
    }
    
    LOG_MAIN_INFO_AT("[LogManager] All logger configurations reloaded successfully");
}

void LogManager::RegisterLogger(const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_); // 保护 loggers_ 线程安全
    auto it = loggers_.find(config.name);
    if (it != loggers_.end()) {
        return;  // 已存在的logger
    }

    // 未初始化，则先初始化
    if (!initialized_) {
        Init();
    }

    // 注册到loggers_
    loggers_[config.name] = std::make_shared<Logger>(config);
}

std::shared_ptr<Logger> LogManager::GetLogger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_); // 保护 loggers_ 线程安全
    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        return it->second;
    }
    return nullptr;
}

void LogManager::RemoveLogger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_); // 保护 loggers_ 线程安全
    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        loggers_.erase(it);
    }
}

bool LogManager::SetLoggerLevel(const std::string& name, spdlog::level::level_enum level) {
    auto logger = GetLogger(name);
    if (logger) {
        logger->SetLevel(level);
        return true;
    }
    return false;
}
bool LogManager::GetLoggerLevel(const std::string& name, spdlog::level::level_enum& level) {
    auto logger = GetLogger(name);
    if (logger) {
        level = logger->GetLevel();
        return true;
    }
    return false;
}

bool LogManager::SetLoggerFormat(const std::string& name, const std::string& format) {
    std::lock_guard<std::mutex> lock(mutex_); // 保护 loggers_ 线程安全
    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        it->second->SetFormat(format);
        return true;
    }
    return false;
}

void LogManager::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_); // 保护 loggers_ 线程安全
    for (auto& logger : loggers_) {
        logger.second->Flush();
    }
}

void LogManager::FlushAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 刷新所有注册的 logger
    for (auto& [name, logger] : loggers_) {
        if (logger) {
            auto spd_logger = logger->GetSpdLogger();
            if (spd_logger) {
                spd_logger->flush();
            }
        }
    }
    
    // 刷新 spdlog 的全局注册表中的所有 logger
    spdlog::apply_all([](std::shared_ptr<spdlog::logger> l) {
        l->flush();
    });
    
    // 短暂延迟，确保异步线程有足够时间处理完队列中的消息
    // 这对于程序即将退出时特别重要
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

LoggerConfig LogManager::ConvertToLoggerConfig(const LogConfig& cfg, const std::string& name) {
    LoggerConfig logger_cfg;
    logger_cfg.name = name;
    
    // 映射日志级别字符串到 spdlog 枚举
    if (cfg.level == "trace") logger_cfg.level = spdlog::level::trace;
    else if (cfg.level == "debug") logger_cfg.level = spdlog::level::debug;
    else if (cfg.level == "info") logger_cfg.level = spdlog::level::info;
    else if (cfg.level == "warn" || cfg.level == "warning") logger_cfg.level = spdlog::level::warn;
    else if (cfg.level == "error") logger_cfg.level = spdlog::level::err;
    else if (cfg.level == "critical") logger_cfg.level = spdlog::level::critical;
    else logger_cfg.level = spdlog::level::info;  // 默认
    
    logger_cfg.log_dir = cfg.dir;
    logger_cfg.write_to_console = cfg.console;
    logger_cfg.is_json = cfg.json_format;
    logger_cfg.async = cfg.async;  // 传递异步配置
    
    // 映射滚动策略
    if (cfg.rotation == "daily") {
        logger_cfg.policy = RotationPolicy::DAILY;
    } else if (cfg.rotation == "filesize") {
        logger_cfg.policy = RotationPolicy::FILESIZE;
    } else {
        logger_cfg.policy = RotationPolicy::NONE;
    }
    
    logger_cfg.max_file_size_mb = cfg.max_file_size_mb;
    logger_cfg.max_files = cfg.max_files;
    
    return logger_cfg;
}

