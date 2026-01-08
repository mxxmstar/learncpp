#include "logger.h"

class LogManager {
public:
    static LogManager& getInstance();

    // 禁止拷贝和赋值
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    /// @brief 初始化日志管理器
    /// @param base_dir 日志根目录 
    /// @param level 日志级别
    /// @param async_threads 异步线程数
    /// @param policy 日志滚动策略
    /// @param max_file_size_mb 最大文件大小（MB）
    /// @param max_files 最大文件数
    void Init(const std::string& base_dir = "./logs", int async_threads = 1);
    
    /// @brief 注册一个logger
    /// @param name logger名称
    /// @param log_file 日志文件名
    void RegisterLogger(const LoggerConfig& config);

    /// @brief 获取一个logger
    /// @param name logger名称
    /// @return logger指针
    std::shared_ptr<Logger> GetLogger(const std::string& name);        

    /// @brief 删除一个logger
    void RemoveLogger(const std::string& name);

    /// @brief 设置logger的日志级别
    bool SetLoggerLevel(const std::string& name, spdlog::level::level_enum level);
    /// @brief 获取logger的日志级别
    bool GetLoggerLevel(const std::string& name, spdlog::level::level_enum& level);
    /// @brief 设置logger的日志格式
    bool SetLoggerFormat(const std::string& name, const std::string& format);
    /// @brief 关闭日志系统
    void Shutdown();

    inline std::shared_ptr<Logger> getMainLogger() { return loggers_["main"]; }

    inline std::shared_ptr<Logger> getErrorLogger() { return loggers_["error"]; }
    
    
private:    
    LogManager() = default;
    ~LogManager() = default;

    /// @brief 已注册的logger集合
    std::unordered_map<std::string, std::shared_ptr<Logger>> loggers_;
    /// @brief 互斥锁
    mutable std::mutex mutex_;

    bool initialized_ = false;
    std::string log_dir_;

    /// @brief 主日志输出目标
    spdlog::sink_ptr main_sink_;
    /// @brief 错误日志输出目标
    spdlog::sink_ptr error_sink_;
    
};

static inline const char* extract_filename(const char* file_path) {
    const char* last_slash = strrchr(file_path, '/');
    return last_slash ? last_slash + 1 : file_path;
}

#define LOG_MAIN LogManager::getInstance().getMainLogger()
#define LOG_ERROR LogManager::getInstance().getErrorLogger()

// 使用可变参数宏确保格式化字符串和参数被正确传递给spdlog
#define LOG_MAIN_DEBUG(...) do { \
    LogManager::getInstance().getMainLogger()->GetSpdLogger()->debug(__VA_ARGS__); \
} while(0)

#define LOG_MAIN_INFO(...) do { \
    LogManager::getInstance().getMainLogger()->GetSpdLogger()->info(__VA_ARGS__); \
} while(0)

#define LOG_MAIN_WARN(...) do { \
    LogManager::getInstance().getMainLogger()->GetSpdLogger()->warn(__VA_ARGS__); \
} while(0)

#define LOG_MAIN_ERROR(...) do { \
    LogManager::getInstance().getMainLogger()->GetSpdLogger()->error(__VA_ARGS__); \
} while(0)

#define LOG_MAIN_CRITICAL(...) do { \
    LogManager::getInstance().getErrorLogger()->GetSpdLogger()->critical(__VA_ARGS__); \
} while(0)

#define LOG_ERROR_DEBUG(...) do { \
    LogManager::getInstance().getErrorLogger()->GetSpdLogger()->debug(__VA_ARGS__); \
} while(0)

#define LOG_ERROR_INFO(...) do { \
    LogManager::getInstance().getErrorLogger()->GetSpdLogger()->info(__VA_ARGS__); \
} while(0)

#define LOG_ERROR_WARN(...) do { \
    LogManager::getInstance().getErrorLogger()->GetSpdLogger()->warn(__VA_ARGS__); \
} while(0)

#define LOG_ERROR_ERROR(...) do { \
    LogManager::getInstance().getErrorLogger()->GetSpdLogger()->error(__VA_ARGS__); \
} while(0)

#define LOG_ERROR_CRITICAL(...) do { \
    LogManager::getInstance().getErrorLogger()->GetSpdLogger()->critical(__VA_ARGS__); \
} while(0)



// 包含文件名和行号的日志宏
#define LOG_MAIN_DEBUG_FL(filename, line, ...) do { \
    LogManager::getInstance().getMainLogger()->GetSpdLogger()->debug("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_MAIN_INFO_FL(filename, line, ...) do { \
    LogManager::getInstance().getMainLogger()->GetSpdLogger()->info("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_MAIN_WARN_FL(filename, line, ...) do { \
    LogManager::getInstance().getMainLogger()->GetSpdLogger()->warn("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_MAIN_ERROR_FL(filename, line, ...) do { \
    LogManager::getInstance().getMainLogger()->GetSpdLogger()->error("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_MAIN_CRITICAL_FL(filename, line, ...) do { \
    LogManager::getInstance().getMainLogger()->GetSpdLogger()->critical("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_ERROR_DEBUG_FL(filename, line, ...) do { \
    LogManager::getInstance().getErrorLogger()->GetSpdLogger()->debug("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_ERROR_INFO_FL(filename, line, ...) do { \
    LogManager::getInstance().getErrorLogger()->GetSpdLogger()->info("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_ERROR_WARN_FL(filename, line, ...) do { \
    LogManager::getInstance().getErrorLogger()->GetSpdLogger()->warn("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_ERROR_ERROR_FL(filename, line, ...) do { \
    LogManager::getInstance().getErrorLogger()->GetSpdLogger()->error("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_ERROR_CRITICAL_FL(filename, line, ...) do { \
    LogManager::getInstance().getErrorLogger()->GetSpdLogger()->critical("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_MAIN_DEBUG_AT(...) LOG_MAIN_DEBUG_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_MAIN_INFO_AT(...) LOG_MAIN_INFO_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_MAIN_WARN_AT(...) LOG_MAIN_WARN_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_MAIN_ERROR_AT(...) LOG_MAIN_ERROR_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_MAIN_CRITICAL_AT(...) LOG_MAIN_CRITICAL_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR_DEBUG_AT(...) LOG_ERROR_DEBUG_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR_INFO_AT(...) LOG_ERROR_INFO_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR_WARN_AT(...) LOG_ERROR_WARN_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR_ERROR_AT(...) LOG_ERROR_ERROR_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR_CRITICAL_AT(...) LOG_ERROR_CRITICAL_FL(__FILE__, __LINE__, __VA_ARGS__)
