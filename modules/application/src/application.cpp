#include "application/application.h"
#include "common/service/iservice.h"
#include "log/logmanager.h"
#include <fstream>
#include <thread>
#include <chrono>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

Application::Application() {
    LOG_MAIN_INFO_AT("[Application] Constructing...");
}

Application::~Application() {
    LOG_MAIN_INFO_AT("[Application] Destructing...");
    
    // 确保完全停止
    if (running_.load()) {
        stop();
    }
}

Application& Application::GetInstance() {
    static Application instance;
    return instance;
}

void Application::OnInit(InitCallback callback) {
    init_callbacks_.push_back(callback);
}

void Application::OnStart(StartCallback callback) {
    start_callbacks_.push_back(callback);
}

void Application::OnStop(StopCallback callback) {
    stop_callbacks_.push_back(callback);
}

std::shared_ptr<IService> Application::GetService(const std::string& name) const {
    auto it = services_.find(name);
    if (it != services_.end()) {
        return it->second;
    }
    return nullptr;
}

int Application::Run() {
    LOG_MAIN_INFO_AT("========================================");
    LOG_MAIN_INFO_AT("Application Starting");
    LOG_MAIN_INFO_AT("========================================");
    
    // 1. 初始化信号处理
    if (!signal_handler_.initialize()) {
        LOG_MAIN_ERROR_AT("[Application] Failed to initialize signal handler");
        return 1;
    }
    
    // 注册信号回调
    signal_handler_.registerCallback(SignalHandler::SIGINT_VAL, [this](int signum) {
        LOG_MAIN_INFO_AT("\n[Application] Received {}", SignalHandler::getSignalName(signum));
        RequestStop();
    });
    
    signal_handler_.registerCallback(SignalHandler::SIGTERM_VAL, [this](int signum) {
        LOG_MAIN_INFO_AT("\n[Application] Received {}", SignalHandler::getSignalName(signum));
        RequestStop();
    });
    
    // 2. 执行初始化阶段
    if (!initialize()) {
        LOG_MAIN_ERROR_AT("[Application] Initialization failed");
        return 1;
    }
    
    // 3. 执行启动阶段
    if (!start()) {
        LOG_MAIN_ERROR_AT("[Application] Start failed");
        stop();
        return 1;
    }
    
    LOG_MAIN_INFO_AT("[Application] Running... (Press Ctrl+C to stop)");
    
    // 4. 主循环：等待停止信号
    while (running_.load() && !signal_handler_.shouldStop()) {
        // 每秒检查一次状态
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // 5. 优雅关闭
    gracefulShutdown();
    
    LOG_MAIN_INFO_AT("========================================");
    LOG_MAIN_INFO_AT("  Application Stopped");
    LOG_MAIN_INFO_AT("========================================");
    
    return 0;
}

void Application::RequestStop() {
    LOG_MAIN_INFO_AT("[Application] Stop requested");
    running_.store(false);
    signal_handler_.requestStop();
}

bool Application::initialize() {
    LOG_MAIN_INFO_AT("[Application] Initializing...");        
    LOG_MAIN_INFO_AT("[Application] Initializing services...");
    for (const auto& name : service_order_) {
        auto service = services_[name];
        if (!service->IsInitialized()) {
            LOG_MAIN_INFO_AT("[Application] Initializing service '{}'", name);
            if (!service->Initialize()) {
                LOG_MAIN_ERROR_AT("[Application] Failed to initialize service '{}'", name);
                return false;
            }
        }
    }    
    
    // 2. 执行所有初始化回调
    for (size_t i = 0; i < init_callbacks_.size(); ++i) {
        try {
            LOG_MAIN_INFO_AT("[Application] Running init callback #{}", (i + 1));
            if (!init_callbacks_[i]()) {
                LOG_MAIN_ERROR_AT("[Application] Init callback #{} failed", (i + 1));
                return false;
            }
        } catch (const std::exception& e) {
            LOG_MAIN_ERROR_AT("[Application] Init callback exception: {}", e.what());
            return false;
        }
    }
    
    initialized_.store(true);
    LOG_MAIN_INFO_AT("[Application] Initialized successfully");
    return true;
}

bool Application::start() {
    LOG_MAIN_INFO_AT("[Application] Starting...");    
    
    LOG_MAIN_INFO_AT("[Application] Starting services...");
    for (const auto& name : service_order_) {
        auto service = services_[name];
        if (!service->IsRunning()) {
            LOG_MAIN_INFO_AT("[Application] Starting service '{}'", name);
            if (!service->Start()) {
                LOG_MAIN_ERROR_AT("[Application] Failed to start service '{}'", name);
                // 启动失败，停止已启动的服务
                stop();
                return false;
            }
        }
    }
    
    
    // 2. 执行所有启动回调
    for (size_t i = 0; i < start_callbacks_.size(); ++i) {
        try {
            LOG_MAIN_INFO_AT("[Application] Running start callback #{}", (i + 1));
            if (!start_callbacks_[i]()) {
                LOG_MAIN_ERROR_AT("[Application] Start callback #{} failed", (i + 1));
                return false;
            }
        } catch (const std::exception& e) {
            LOG_MAIN_ERROR_AT("[Application] Start callback exception: {}", e.what());
            return false;
        }
    }
    
    running_.store(true);
    LOG_MAIN_INFO_AT("[Application] Started successfully");
    return true;
}

void Application::stop() {
    if (!running_.load()) {
        return;
    }
        
    LOG_MAIN_INFO_AT("[Application] Stopping services...");    
    for (auto it = service_order_.rbegin(); it != service_order_.rend(); ++it) {
        const auto& name = *it;
        auto service = services_[name];
        
        if (service->IsRunning()) {            
            LOG_MAIN_INFO_AT("[Application] Stopping service '{}'", name);
            service->Stop();
        }
    }
    
    
    // 2. 执行所有停止回调（逆序执行）
    for (auto it = stop_callbacks_.rbegin(); it != stop_callbacks_.rend(); ++it) {
        try {
            (*it)();
        } catch (const std::exception& e) {
            LOG_MAIN_ERROR_AT("[Application] Stop callback exception: {}", e.what());
        }
    }
    
    running_.store(false);
    LOG_MAIN_INFO_AT("[Application] Stopped");
}

void Application::gracefulShutdown() {
    LOG_MAIN_INFO_AT("\n[Application] Graceful shutdown initiated...");
    
    // 1. 停止接收新请求
    running_.store(false);
    
    // 2. 等待正在处理的任务完成（最多等待 10 秒）
    auto shutdown_start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(10);
    
    LOG_MAIN_INFO_AT("[Application] Waiting for tasks to complete...");
    
    // 3. 执行清理
    stop();
    
    // 4. 计算关闭耗时
    auto shutdown_duration = std::chrono::steady_clock::now() - shutdown_start;
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(shutdown_duration).count();
    
    LOG_MAIN_INFO_AT("[Application] Shutdown completed in {}ms", duration_ms);
}
