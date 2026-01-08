#include "log/logmanager.h"

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

    // 创建日志器
    // loggers_["error"] = std::make_shared<spdlog::logger>("error", std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_dir_ + "error.log")); 
    // loggers_["main"] = std::make_shared<spdlog::logger>("main", std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_dir_ + "main.log"));   
    auto main_config = LoggerConfig("main", spdlog::level::trace);
    loggers_["main"] = std::make_shared<Logger>(main_config);
    auto error_config = LoggerConfig("error", spdlog::level::err);
    loggers_["error"] = std::make_shared<Logger>(error_config);
    initialized_ = true;
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

