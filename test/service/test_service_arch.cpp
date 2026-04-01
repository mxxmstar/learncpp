#include <iostream>
#include <csignal>
#include <thread>
#include "config/common_config.h"
#include "log/logmanager.h"
#include "service/service_container.h"
#include "service/http_server_service.h"
#include "service/zlm_service.h"

// 全局信号标志
std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "\n[Signal] Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

/// @brief 简单示例：只使用 HTTP 服务器
int testHttpServerOnly() {
    try {
        std::cout << "=== Test: HTTP Server Only ===" << std::endl;
        
        // 1. 加载配置
        ConfigManager& config_mgr = ConfigManager::getInstance();
        if (!config_mgr.load("tools/config.yaml")) {
            std::cerr << "Failed to load config" << std::endl;
            return 1;
        }
        
        const auto& config = config_mgr.getConfig();
        
        // 2. 初始化日志
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init(config.log.dir, 1);
        
        std::cout << "[Config] Loaded from: " << config_mgr.getConfigPath() << std::endl;
        std::cout << "[Server] Host: " << config.server.host 
                  << ", Port: " << config.server.port << std::endl;
        
        // 3. 创建服务容器
        auto& container = ServiceContainer::getInstance();
        
        // 4. 注册 HTTP 服务器服务
        container.registerService<HttpServerService>(config.server);
        
        // 5. 初始化所有服务
        std::cout << "[Init] Initializing services..." << std::endl;
        if (!container.initializeAll()) {
            LOG_MAIN_ERROR_AT("Failed to initialize services");
            return 1;
        }
        
        // 6. 启动所有服务
        std::cout << "[Start] Starting services..." << std::endl;
        if (!container.startAll()) {
            LOG_MAIN_ERROR_AT("Failed to start services");
            return 1;
        }
        
        std::cout << "[Running] HTTP Server is running. Press Ctrl+C to stop." << std::endl;
        
        // 7. 获取服务并使用
        auto http_svc = container.getService<HttpServerService>();
        if (http_svc) {
            std::cout << "[Service] HTTP Server service obtained" << std::endl;
            std::cout << "[Service] IO Context: " << (http_svc->getIoContext() ? "valid" : "null") << std::endl;
        }
        
        // 8. 运行 io_context
        if (http_svc && http_svc->getIoContext()) {
            http_svc->getIoContext()->run();
        }
        
        // 9. 等待退出信号
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // 10. 停止所有服务
        std::cout << "[Stop] Stopping all services..." << std::endl;
        container.stopAll();
        
        // 11. 清理
        log_mgr.Shutdown();
        
        std::cout << "[Exit] Application exited gracefully" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

/// @brief 完整示例：HTTP 服务器 + ZLMediaKit
int testWithZLM() {
    try {
        std::cout << "=== Test: HTTP Server + ZLMediaKit ===" << std::endl;
        
        // 1. 加载配置
        ConfigManager& config_mgr = ConfigManager::getInstance();
        if (!config_mgr.load("tools/config.yaml")) {
            std::cerr << "Failed to load config" << std::endl;
            return 1;
        }
        
        const auto& config = config_mgr.getConfig();
        
        // 2. 初始化日志
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init(config.log.dir, 1);
        
        std::cout << "[Config] Loaded from: " << config_mgr.getConfigPath() << std::endl;
        std::cout << "[Server] Port: " << config.server.port << std::endl;
        std::cout << "[Media] ZLM Port: " << config.media.zlm_port << std::endl;
        
        // 3. 创建服务容器
        auto& container = ServiceContainer::getInstance();
        
        // 4. 注册 HTTP 服务器服务
        container.registerService<HttpServerService>(config.server);
        
        // 5. 创建共享的 io_context
        boost::asio::io_context shared_ctx;
        
        // 6. 注册 ZLMediaKit 服务
        container.registerService<ZLMService>(shared_ctx, config.media);
        
        // 7. 初始化所有服务
        std::cout << "[Init] Initializing " << container.getServiceCount() << " services..." << std::endl;
        if (!container.initializeAll()) {
            LOG_MAIN_ERROR_AT("Failed to initialize services");
            return 1;
        }
        
        // 8. 启动所有服务
        std::cout << "[Start] Starting " << container.getServiceCount() << " services..." << std::endl;
        if (!container.startAll()) {
            LOG_MAIN_ERROR_AT("Failed to start services");
            return 1;
        }
        
        std::cout << "[Running] All services started. Press Ctrl+C to stop." << std::endl;
        std::cout << "[Services] Registered services:" << std::endl;
        for (const auto& name : container.getServiceNames()) {
            std::cout << "  - " << name << std::endl;
        }
        
        // 9. 获取服务
        auto http_svc = container.getService<HttpServerService>();
        auto zlm_svc = container.getService<ZLMService>();
        
        if (http_svc && zlm_svc) {
            std::cout << "[Service] Both services obtained successfully" << std::endl;
            
            // 可以在这里访问服务的底层对象
            auto zlm_mgr = zlm_svc->getZLMManager();
            if (zlm_mgr) {
                std::cout << "[Service] ZLMManager is available" << std::endl;
            }
        }
        
        // 10. 运行 HTTP 服务器的 io_context
        if (http_svc && http_svc->getIoContext()) {
            std::cout << "[Run] Running HTTP server io_context..." << std::endl;
            http_svc->getIoContext()->run();
        }
        
        // 11. 等待退出信号
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // 12. 停止所有服务
        std::cout << "[Stop] Stopping all services..." << std::endl;
        container.stopAll();
        
        // 13. 清理
        log_mgr.Shutdown();
        
        std::cout << "[Exit] Application exited gracefully" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        LOG_MAIN_CRITICAL_AT("Fatal error: {}", e.what());
        return 1;
    }
}

int main() {
    // 设置信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // 选择要运行的测试
    // 返回 0: 只运行 HTTP 服务器
    // 返回 1: 运行 HTTP 服务器 + ZLMediaKit
    const int test_mode = 1;
    
    if (test_mode == 0) {
        return testHttpServerOnly();
    } else {
        return testWithZLM();
    }
}
