#include "application/signal_handler.h"
#include "log/logmanager.h"
#include <csignal>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/types.h>
#endif

SignalHandler* SignalHandler::instance_ = nullptr;

SignalHandler::SignalHandler() {
    if (instance_ != nullptr) {
        LOG_MAIN_WARN_AT("[SignalHandler] Multiple instances created!");
    }
    instance_ = this;
}

SignalHandler::~SignalHandler() {
    // 恢复默认信号处理
#ifdef _WIN32
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
#else
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
#endif
    instance_ = nullptr;
}

bool SignalHandler::initialize() {
#ifdef _WIN32
    // Windows: 注册控制台信号处理
    if (signal(SIGINT, platformSignalHandler) == SIG_ERR) {
        LOG_MAIN_ERROR_AT("[SignalHandler] Failed to register SIGINT handler");
        return false;
    }
    
    if (signal(SIGTERM, platformSignalHandler) == SIG_ERR) {
        LOG_MAIN_ERROR_AT("[SignalHandler] Failed to register SIGTERM handler");
        return false;
    }
    
    // Windows 也支持 Ctrl+Break
    if (signal(SIGBREAK, platformSignalHandler) == SIG_ERR) {
        LOG_MAIN_WARN_AT("[SignalHandler] Failed to register SIGBREAK handler");
        // 不返回 false，因为这不是关键信号
    }
#else
    // Linux/macOS: 注册 POSIX 信号
    struct sigaction sa;
    sa.sa_handler = platformSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  // 不使用 SA_RESTART，让系统调用可以被中断
    
    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        LOG_MAIN_ERROR_AT("[SignalHandler] Failed to register SIGINT handler");
        return false;
    }
    
    if (sigaction(SIGTERM, &sa, nullptr) == -1) {
        LOG_MAIN_ERROR_AT("[SignalHandler] Failed to register SIGTERM handler");
        return false;
    }
    
    // SIGHUP: 重新加载配置
    if (sigaction(SIGHUP, &sa, nullptr) == -1) {
        LOG_MAIN_WARN_AT("[SignalHandler] Failed to register SIGHUP handler");
        // 不返回 false
    }
#endif
    
    LOG_MAIN_INFO_AT("[SignalHandler] Initialized successfully");
    return true;
}

void SignalHandler::registerCallback(int signum, SignalCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callbacks_[signum] = callback;
}

void SignalHandler::unregisterCallback(int signum) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callbacks_.erase(signum);
}

void SignalHandler::requestStop() {
    stop_requested_.store(true);
    last_signal_.store(SIGTERM_VAL);
    notifyWaiters();
    
    // 触发所有回调
    std::lock_guard<std::mutex> lock(callback_mutex_);
    for (auto& [signum, callback] : callbacks_) {
        try {
            callback(signum);
        } catch (const std::exception& e) {
            LOG_MAIN_ERROR_AT("[SignalHandler] Callback exception: {}", e.what());
        }
    }
}

void SignalHandler::reset() {
    stop_requested_.store(false);
    last_signal_.store(0);
}

void SignalHandler::waitForSignal() {
    std::unique_lock<std::mutex> lock(wait_mutex_);
    wait_cv_.wait(lock, [this]() {
        return stop_requested_.load();
    });
}

std::string SignalHandler::getSignalName(int signum) {
    switch (signum) {
        case SIGINT_VAL:
            return "SIGINT (Ctrl+C)";
        case SIGTERM_VAL:
            return "SIGTERM (Terminate)";
#ifdef _WIN32
        case SIGBREAK:
            return "SIGBREAK (Ctrl+Break)";
#else
        case SIGHUP_VAL:
            return "SIGHUP (Hangup/Reload)";
        case SIGUSR1_VAL:
            return "SIGUSR1 (User Defined 1)";
        case SIGUSR2_VAL:
            return "SIGUSR2 (User Defined 2)";
#endif
        default:
            return "Unknown Signal (" + std::to_string(signum) + ")";
    }
}

void SignalHandler::platformSignalHandler(int signum) {
    if (instance_) {
        instance_->last_signal_.store(signum);
        instance_->stop_requested_.store(true);
        instance_->notifyWaiters();
        
        // 在信号处理器中只执行异步安全的操作
        // 回调会在主线程中通过 waitForSignal 或轮询触发
        
#ifdef _WIN32
        LOG_MAIN_INFO_AT("\n[SignalHandler] Received {}", getSignalName(signum));
#else
        // Linux 下 write 是异步安全的，cout 不是
        const char* msg = "\n[SignalHandler] Received signal\n";
        write(STDOUT_FILENO, msg, strlen(msg));
#endif
    }
}

void SignalHandler::notifyWaiters() {
    wait_cv_.notify_all();
}
