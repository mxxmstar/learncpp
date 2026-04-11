#include <iostream>
#include <csignal>
#include <thread>
// #include "config/common_config.h"
// #include "log/logmanager.h"
// #include "service/service_container.h"
// #include "service/http_server_service.h"
// #include "service/zlm_service.h"

// // 全局信号标志
// std::atomic<bool> g_running{true};

// void signalHandler(int signal) {
//     LOG_MAIN_INFO_AT("Received signal {}, shutting down...", signal);
//     g_running = false;
// }

int main() {
    // try {
    //     // 1. 设置信号处理
    //     std::signal(SIGINT, signalHandler);
    //     std::signal(SIGTERM, signalHandler);
        
    //     // 2. 加载配置
    //     ConfigManager& config_mgr = ConfigManager::getInstance();
    //     if (!config_mgr.load("tools/config.yaml")) {
    //         std::cerr << "Failed to load config" << std::endl;
    //         return 1;
    //     }
        
    //     const auto& config = config_mgr.getConfig();
        
    //     // 3. 初始化日志
    //     LogManager& log_mgr = LogManager::getInstance();
    //     // 使用 mainlog 配置初始化
    //     if (config.logs.count("mainlog") > 0) {
    //         log_mgr.Init(config.logs.at("mainlog").dir, 1);
    //     } else {
    //         log_mgr.Init("./logs", 1);
    //     }
        
    //     LOG_MAIN_INFO_AT("========================================");
    //     LOG_MAIN_INFO_AT("Application starting...");
    //     LOG_MAIN_INFO_AT("Config loaded from: {}", config_mgr.getConfigPath());
    //     LOG_MAIN_INFO_AT("========================================");
        
    //     // 4. 创建服务容器
    //     auto& container = ServiceContainer::getInstance();
        
    //     // 5. 注册 HTTP 服务器服务
    //     auto http_service = std::make_shared<HttpServerService>(config.server);
    //     container.registerService<HttpServerService>(config.server);
        
    //     // 6. 创建共享的 io_context（用于 ZLM 等服务）
    //     boost::asio::io_context shared_ctx;
        
    //     // 7. 注册 ZLMediaKit 服务
    //     container.registerService<ZLMService>(shared_ctx, config.zlm);
        
    //     // 8. 初始化所有服务
    //     LOG_MAIN_INFO_AT("Initializing {} services...", container.getServiceCount());
    //     if (!container.initializeAll()) {
    //         LOG_MAIN_ERROR_AT("Failed to initialize services");
    //         return 1;
    //     }
        
    //     // 9. 启动所有服务
    //     LOG_MAIN_INFO_AT("Starting {} services...", container.getServiceCount());
    //     if (!container.startAll()) {
    //         LOG_MAIN_ERROR_AT("Failed to start services");
    //         return 1;
    //     }
        
    //     LOG_MAIN_INFO_AT("========================================");
    //     LOG_MAIN_INFO_AT("All services started successfully");
    //     LOG_MAIN_INFO_AT("Application is running. Press Ctrl+C to stop.");
    //     LOG_MAIN_INFO_AT("========================================");
        
    //     // 10. 运行 HTTP 服务器的 io_context（在主线程中）
    //     auto http_svc = container.getService<HttpServerService>();
    //     if (http_svc && http_svc->getIoContext()) {
    //         LOG_MAIN_INFO_AT("Running HTTP server io_context...");
    //         http_svc->getIoContext()->run();
    //     }
        
    //     // 11. 等待退出信号（如果 io_context 返回）
    //     while (g_running) {
    //         std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //     }
        
    //     // 12. 停止所有服务
    //     LOG_MAIN_INFO_AT("========================================");
    //     LOG_MAIN_INFO_AT("Shutting down application...");
    //     container.stopAll();
        
    //     // 13. 清理
    //     log_mgr.Shutdown();
        
    //     LOG_MAIN_INFO_AT("Application exited gracefully");
    //     LOG_MAIN_INFO_AT("========================================");
        
    //     return 0;
        
    // } catch (const std::exception& e) {
    //     std::cerr << "Fatal error: " << e.what() << std::endl;
    //     LOG_MAIN_CRITICAL_AT("Fatal error: {}", e.what());
    //     return 1;
    // }
    std::cout << "Hello World!" << std::endl;
}
