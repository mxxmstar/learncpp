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
    /// @return 注册的logger指针
    std::shared_ptr<spdlog::logger> RegisterLogger(const LoggerConfig& config);

    /// @brief 获取一个logger
    /// @param name logger名称
    /// @return logger指针
    std::shared_ptr<spdlog::logger> GetLogger(const std::string& name);        

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

    inline std::shared_ptr<spdlog::logger> getMainLogger() { return loggers_["main"]; }

    inline std::shared_ptr<spdlog::logger> getErrorLogger() { return loggers_["error"]; }
    
    
private:    
    LogManager() = default;
    ~LogManager() = default;

    /// @brief 已注册的logger集合
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers_;
    /// @brief 互斥锁
    mutable std::mutex mutex_;

    bool initialized_ = false;
    std::string log_dir_;

    /// @brief 主日志输出目标
    spdlog::sink_ptr main_sink_;
    /// @brief 错误日志输出目标
    spdlog::sink_ptr error_sink_;
    
};