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

    // 创建日志目录
    // if (!std::filesystem::exists(log_dir_)) {
    //     std::filesystem::create_directories(log_dir_);
    // }

    // 创建日志器
    loggers_["error"] = std::make_shared<spdlog::logger>("error", std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_dir_ + "error.log")); 
    loggers_["main"] = std::make_shared<spdlog::logger>("main", std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_dir_ + "main.log"));   
    initialized_ = true;
}


std::shared_ptr<spdlog::logger> LogManager::RegisterLogger(const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_); // 保护 loggers_ 线程安全
    auto it = loggers_.find(config.name);
    if (it != loggers_.end()) {
        return it->second;  // 已存在的logger
    }

    // 未初始化，则先初始化
    if (!initialized_) {
        Init();
    }

    // 创建新的logger
    Logger logger(config);
    auto spd_logger = logger.GetLogger();

    // 注册到loggers_
    loggers_[config.name] = spd_logger;

    return spd_logger;
}

std::shared_ptr<spdlog::logger> LogManager::GetLogger(const std::string& name) {
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
        logger->set_level(level);
        return true;
    }
    return false;
}
bool LogManager::GetLoggerLevel(const std::string& name, spdlog::level::level_enum& level) {
    auto logger = GetLogger(name);
    if (logger) {
        level = logger->level();
        return true;
    }
    return false;
}

bool LogManager::SetLoggerFormat(const std::string& name, const std::string& format) {
    std::lock_guard<std::mutex> lock(mutex_); // 保护 loggers_ 线程安全
    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        it->second->set_pattern(format);
        return true;
    }
    return false;
}

void LogManager::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_); // 保护 loggers_ 线程安全
    for (auto& logger : loggers_) {
        logger.second->flush();
    }
}

