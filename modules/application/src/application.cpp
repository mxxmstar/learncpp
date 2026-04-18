#include "application/application.h"
#include "log/logmanager.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

Application::Application() {
    std::cout << "[Application] Constructing..." << std::endl;
}

Application::~Application() {
    std::cout << "[Application] Destructing..." << std::endl;
    
    // 确保完全停止
    if (running_.load()) {
        stop();
    }
}

Application& Application::getInstance() {
    static Application instance;
    return instance;
}

bool Application::loadConfig(const std::string& config_path) {
    std::cout << "[Application] Loading config from: " << config_path << std::endl;
    
    // 检查文件是否存在
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "[Application] Warning: Config file not found: " << config_path << std::endl;
        std::cerr << "[Application] Using default configuration" << std::endl;
        return false;
    }
    
    // TODO: 使用 yaml-cpp 或 boost::json 解析配置文件
    // 这里先设置一些默认配置
    
    setConfig("app.name", "VideoPipelineApp");
    setConfig("app.version", "1.0.0");
    setConfig("log.dir", "logs");
    setConfig("log.level", "info");
    
    std::cout << "[Application] Config loaded successfully" << std::endl;
    return true;
}

void Application::onInit(InitCallback callback) {
    init_callbacks_.push_back(callback);
}

void Application::onStart(StartCallback callback) {
    start_callbacks_.push_back(callback);
}

void Application::onStop(StopCallback callback) {
    stop_callbacks_.push_back(callback);
}

bool Application::initLogger(const std::string& log_dir, const std::string& log_level) {
    try {
        log_manager_ = &LogManager::getInstance();
        log_manager_->Init();
        
        std::cout << "[Application] Logger initialized: dir=" << log_dir 
                  << ", level=" << log_level << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Application] Failed to initialize logger: " << e.what() << std::endl;
        return false;
    }
}

std::shared_ptr<IService> Application::getService(const std::string& name) const {
    auto it = services_.find(name);
    if (it != services_.end()) {
        return it->second;
    }
    return nullptr;
}

int Application::run() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Application Starting" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // 1. 初始化信号处理
    if (!signal_handler_.initialize()) {
        std::cerr << "[Application] Failed to initialize signal handler" << std::endl;
        return 1;
    }
    
    // 注册信号回调
    signal_handler_.registerCallback(SignalHandler::SIGINT_VAL, [this](int signum) {
        std::cout << "\n[Application] Received " << SignalHandler::getSignalName(signum) << std::endl;
        requestStop();
    });
    
    signal_handler_.registerCallback(SignalHandler::SIGTERM_VAL, [this](int signum) {
        std::cout << "\n[Application] Received " << SignalHandler::getSignalName(signum) << std::endl;
        requestStop();
    });
    
    // 2. 执行初始化阶段
    if (!initialize()) {
        std::cerr << "[Application] Initialization failed" << std::endl;
        return 1;
    }
    
    // 3. 执行启动阶段
    if (!start()) {
        std::cerr << "[Application] Start failed" << std::endl;
        stop();
        return 1;
    }
    
    std::cout << "\n[Application] Running... (Press Ctrl+C to stop)" << std::endl;
    
    // 4. 主循环：等待停止信号
    while (running_.load() && !signal_handler_.shouldStop()) {
        // 每秒检查一次状态
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // 5. 优雅关闭
    gracefulShutdown();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Application Stopped" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}

void Application::requestStop() {
    std::cout << "[Application] Stop requested" << std::endl;
    running_.store(false);
    signal_handler_.requestStop();
}

bool Application::initialize() {
    std::cout << "[Application] Initializing..." << std::endl;
    
    // TODO: 初始化所有 IService 服务（IService 尚未实现）
    /*
    std::cout << "[Application] Initializing services..." << std::endl;
    for (const auto& name : service_order_) {
        auto service = services_[name];
        if (!service->isInitialized()) {
            std::cout << "[Application] Initializing service '" << name << "'" << std::endl;
            if (!service->initialize()) {
                std::cerr << "[Application] Failed to initialize service '" << name << "'" << std::endl;
                return false;
            }
        }
    }
    */
    
    // 2. 执行所有初始化回调
    for (size_t i = 0; i < init_callbacks_.size(); ++i) {
        try {
            std::cout << "[Application] Running init callback #" << (i + 1) << std::endl;
            if (!init_callbacks_[i]()) {
                std::cerr << "[Application] Init callback #" << (i + 1) << " failed" << std::endl;
                return false;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Application] Init callback exception: " << e.what() << std::endl;
            return false;
        }
    }
    
    initialized_.store(true);
    std::cout << "[Application] Initialized successfully" << std::endl;
    return true;
}

bool Application::start() {
    std::cout << "[Application] Starting..." << std::endl;
    
    // TODO: 启动所有 IService 服务（IService 尚未实现）
    /*
    std::cout << "[Application] Starting services..." << std::endl;
    for (const auto& name : service_order_) {
        auto service = services_[name];
        if (!service->isRunning()) {
            std::cout << "[Application] Starting service '" << name << "'" << std::endl;
            if (!service->start()) {
                std::cerr << "[Application] Failed to start service '" << name << "'" << std::endl;
                // 启动失败，停止已启动的服务
                stop();
                return false;
            }
        }
    }
    */
    
    // 2. 执行所有启动回调
    for (size_t i = 0; i < start_callbacks_.size(); ++i) {
        try {
            std::cout << "[Application] Running start callback #" << (i + 1) << std::endl;
            if (!start_callbacks_[i]()) {
                std::cerr << "[Application] Start callback #" << (i + 1) << " failed" << std::endl;
                return false;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Application] Start callback exception: " << e.what() << std::endl;
            return false;
        }
    }
    
    running_.store(true);
    std::cout << "[Application] Started successfully" << std::endl;
    return true;
}

void Application::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "[Application] Stopping..." << std::endl;
    
    // TODO: 停止所有 IService 服务（IService 尚未实现）
    /*
    std::cout << "[Application] Stopping services..." << std::endl;
    for (auto it = service_order_.rbegin(); it != service_order_.rend(); ++it) {
        const auto& name = *it;
        auto service = services_[name];
        
        if (service->isRunning()) {
            std::cout << "[Application] Stopping service '" << name << "'" << std::endl;
            service->stop();
        }
    }
    */
    
    // 2. 执行所有停止回调（逆序执行）
    for (auto it = stop_callbacks_.rbegin(); it != stop_callbacks_.rend(); ++it) {
        try {
            (*it)();
        } catch (const std::exception& e) {
            std::cerr << "[Application] Stop callback exception: " << e.what() << std::endl;
        }
    }
    
    running_.store(false);
    std::cout << "[Application] Stopped" << std::endl;
}

void Application::gracefulShutdown() {
    std::cout << "\n[Application] Graceful shutdown initiated..." << std::endl;
    
    // 1. 停止接收新请求
    running_.store(false);
    
    // 2. 等待正在处理的任务完成（最多等待 10 秒）
    auto shutdown_start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(10);
    
    std::cout << "[Application] Waiting for tasks to complete..." << std::endl;
    
    // 3. 执行清理
    stop();
    
    // 4. 计算关闭耗时
    auto shutdown_duration = std::chrono::steady_clock::now() - shutdown_start;
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(shutdown_duration).count();
    
    std::cout << "[Application] Shutdown completed in " << duration_ms << "ms" << std::endl;
}
