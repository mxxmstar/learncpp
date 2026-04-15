#pragma once

#include <functional>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

/// @brief 跨平台信号处理器
/// 支持 Windows 和 Linux/macOS 的信号处理
class SignalHandler {
public:
    using SignalCallback = std::function<void(int)>;
    
    SignalHandler();
    ~SignalHandler();
    
    /// @brief 初始化信号处理（必须在主线程调用）
    bool initialize();
    
    /// @brief 注册信号回调
    /// @param signum 信号编号
    /// @param callback 回调函数
    void registerCallback(int signum, SignalCallback callback);
    
    /// @brief 移除信号回调
    void unregisterCallback(int signum);
    
    /// @brief 检查是否收到停止信号
    bool shouldStop() const { return stop_requested_.load(); }
    
    /// @brief 请求停止（触发所有回调）
    void requestStop();
    
    /// @brief 重置停止标志
    void reset();
    
    /// @brief 获取最后一次收到的信号
    int getLastSignal() const { return last_signal_.load(); }
    
    /// @brief 等待信号（阻塞直到收到信号）
    void waitForSignal();
    
    /// @brief 获取信号名称
    static std::string getSignalName(int signum);
    
    // 常用信号常量
#ifdef _WIN32
    static constexpr int SIGINT_VAL = 2;    // Ctrl+C
    static constexpr int SIGTERM_VAL = 15;  // 终止
#else
    static constexpr int SIGINT_VAL = 2;    // SIGINT
    static constexpr int SIGTERM_VAL = 15;  // SIGTERM
    static constexpr int SIGHUP_VAL = 1;    // SIGHUP (reload config)
    static constexpr int SIGUSR1_VAL = 10;  // 用户自定义1
    static constexpr int SIGUSR2_VAL = 12;  // 用户自定义2
#endif
    
private:
    /// @brief 平台特定的信号处理函数
    static void platformSignalHandler(int signum);
    
    /// @brief 通知所有等待者
    void notifyWaiters();
    
    std::atomic<bool> stop_requested_{false};
    std::atomic<int> last_signal_{0};
    
    std::mutex callback_mutex_;
    std::map<int, SignalCallback> callbacks_;
    
    // 用于 waitForSignal
    std::mutex wait_mutex_;
    std::condition_variable wait_cv_;
    
    // 单例模式（信号处理需要全局访问）
    static SignalHandler* instance_;
};
