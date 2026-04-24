#include <iostream>
#include <csignal>
#include <thread>
#include "common/config/common_config.h"
#include "common/log/logmanager.h"
#include "application/service_container.h"
#include "service/http_server/http_server_service.h"
#include "service/zlm/zlm_service.h"
#include "service/http_client/http_client_pool_service.h"
#include "api/api_router_registrar.h"

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
        
        // 初始化日志
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();

        // 1. 加载配置
        ConfigManager& config_mgr = ConfigManager::GetInstance();
        if (!config_mgr.Load("tools/config.yaml")) {
            std::cerr << "Failed to load config" << std::endl;
            return 1;
        }
        
        const auto& config = config_mgr.GetConfig();
        
        // 2. 批量初始化日志
        if (!config.logs.empty()) {
            log_mgr.ReloadFromConfigs(config.logs);
        } else {
            LoggerConfig default_cfg;
            default_cfg.name = "main";
            default_cfg.level = spdlog::level::info;
            log_mgr.ReloadFromConfig(default_cfg);
        }
        
        std::cout << "[Config] Loaded from: " << config_mgr.GetConfigPath() << std::endl;
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
        
        // 1. 初始化日志（第一阶段：简单初始化，不依赖配置）
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init("./logs", 1);  // 先简单初始化，使用默认目录
        
        // 2. 加载配置
        ConfigManager& config_mgr = ConfigManager::GetInstance();
        std::string config_path = "../tools/config.yaml";
        std::cout << "[Config] Trying to load from: " << config_path << std::endl;
        if (!config_mgr.Load(config_path)) {
            LOG_MAIN_ERROR_AT("Failed to load config from {}", config_path);
            std::cerr << "[Error] Failed to load config from: " << config_path << std::endl;
            std::cerr << "[Hint] Current working directory: " << std::filesystem::current_path().string() << std::endl;
            return 1;
        }
        
        const auto& config = config_mgr.GetConfig();
        std::cout << "[Config] ZLM host: " << config.zlm.zlm_host << std::endl;
        std::cout << "[Config] ZLM port: " << config.zlm.zlm_port << std::endl;
        std::cout << "[Config] ZLM secret: " << config.zlm.secret << std::endl;
        
        // 3. 批量重新加载日志配置（第二阶段：使用配置文件）
        if (!config.logs.empty()) {
            log_mgr.ReloadFromConfigs(config.logs);
        } else {
            LoggerConfig default_cfg;
            default_cfg.name = "main";
            default_cfg.level = spdlog::level::info;
            log_mgr.ReloadFromConfig(default_cfg);
        }
        LOG_MAIN_INFO_AT("Application starting...");
        LOG_MAIN_INFO_AT("Config loaded from: {}", config_mgr.GetConfigPath());
    
        
        // 4. 创建服务容器
        auto& container = ServiceContainer::getInstance();
        
        // 5. 创建共享的 io_context
        boost::asio::io_context shared_ctx;
        
        // 5. 注册 HTTP 服务器服务
        container.registerService<HttpServerService>(config.server);
        
        // 6. 注册 HttpClientPool 服务（ZLM 依赖它）
        container.registerService<HttpClientPoolService>(config.zlm_client);
        
        // 7. 注册 ZLMediaKit 服务
        // 注意：此测试使用旧的 ServiceContainer 架构，ZLMService 需要修改以兼容
        // TODO: 更新为使用 Application 架构
        /*
        auto http_pool_svc = container.getService<HttpClientPoolService>();
        if (http_pool_svc) {
            container.registerService<ZLMService>(shared_ctx, http_pool_svc->GetHttpClientPool(), config.zlm);
        } else {
            LOG_MAIN_ERROR_AT("Failed to get HttpClientPoolService");
            return 1;
        }
        */
        
        // 8. 初始化所有服务
        std::cout << "[Init] Initializing " << container.getServiceCount() << " services..." << std::endl;
        if (!container.initializeAll()) {
            LOG_MAIN_ERROR_AT("Failed to initialize services");
            return 1;
        }
        
        // 4. 注册所有 API 路由
        ApiRouterRegistrar::RegisterAllRoutes();
        std::cout << "[Server] Port: " << config.server.port << std::endl;
        std::cout << "[ZLM] ZLM Port: " << config.zlm.zlm_port << std::endl;
        std::cout << "[ZLM] ZLM Secret: " << config.zlm.secret << std::endl;

        // 9. 启动所有服务
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
        
        // 10. 获取服务
        auto http_svc = container.getService<HttpServerService>();
        // auto zlm_svc = container.getService<ZLMService>();  // TODO: ZLMService 需要适配 ServiceContainer
        
        if (http_svc /*&& zlm_svc*/) {
            std::cout << "[Service] HTTP service obtained successfully" << std::endl;
            
            // TODO: ZLMService 需要适配 ServiceContainer
            /*
            auto zlm_mgr = zlm_svc->GetZLMManager();
            if (zlm_mgr) {
                std::cout << "[Service] ZLMManager is available" << std::endl;
            }
            */
        }
        
        // 11. 运行 HTTP 服务器的 io_context
        if (http_svc && http_svc->getIoContext()) {
            std::cout << "[Run] Running HTTP server io_context..." << std::endl;
            http_svc->getIoContext()->run();
        }
        
        //// 12. 等待退出信号
        while (g_running) {
           std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // 13. 停止所有服务
        std::cout << "[Stop] Stopping all services..." << std::endl;
        container.stopAll();
        
        // 14. 清理
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
