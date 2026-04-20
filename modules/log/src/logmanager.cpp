#include "log/logmanager.h"
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
    auto main_config = LoggerConfig("main", spdlog::level::trace);
    loggers_["main"] = std::make_shared<Logger>(main_config);
    auto error_config = LoggerConfig("error", spdlog::level::err);
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

// // 新增重载版本：直接从 LogConfig 重新加载
// void LogManager::ReloadFromConfig(const LogConfig& config) {
//     // 使用转换接口将 LogConfig 转换为 LoggerConfig
//     ReloadFromConfig(config.toLoggerConfig("main"));
// }


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

