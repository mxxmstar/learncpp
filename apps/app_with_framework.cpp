#include "application/application.h"
#include "config/common_config.h"
// #include "video_pipeline/video_pipeline.h"  // 暂时注释，videopipeline 模块未启用
#include "log/logmanager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>

#include "common/service/iservice.h"
#include "zlmediakit/service/zlm_service.h"
#include "web/service/httpclient_pool_service.h"
#include "web/service/http_server_service.h"
#include "web/api/api_router_registrar.h"

int main() {
    try {
        // === 第一次初始化：LogManager 简单初始化 ===
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init("./logs", 1);

        LOG_MAIN_INFO_AT("Application starting...");
        log_mgr.FlushAll();  // 确保启动日志输出

        // === Config 加载（现在可以使用日志宏）===
        ConfigManager& config_mgr = ConfigManager::GetInstance();
        
        if (!config_mgr.Load("../tools/config.yaml")) {
            LOG_MAIN_WARN_AT("Failed to load config, using defaults");
        } else {
            LOG_MAIN_INFO_AT("Config loaded successfully");
            LOG_MAIN_INFO_AT("Logs count: {}", config_mgr.GetConfig().logs.size());
            for (const auto& [name, cfg] : config_mgr.GetConfig().logs) {
                LOG_MAIN_INFO_AT("  - {}: level={}, dir={}", name, cfg.level, cfg.dir);
            }
        }        
        
        // 打印配置详情
        config_mgr.Dump();        
        
        LOG_MAIN_INFO_AT("Application exiting...");
        // 批量重新加载所有日志配置
        log_mgr.ReloadFromConfigs(config_mgr.GetConfig().logs);

        // === 4. 获取 Application 实例 ===
        auto& app = Application::GetInstance();
        
        // === 5. 注册 IService 服务 ===
        // 从配置中获取 HTTP 服务器配置
        const auto& config = config_mgr.GetConfig();
        HttpServerConfig http_config;
        http_config.host = config.server.host;
        http_config.port = config.server.port;
        
        app.RegisterService<HttpServerService>(http_config);
        /*app.RegisterService<ZLMService>();
        app.RegisterService<HttpClientPoolService>();*/
        
        // 初始化
        app.OnInit([&app]() {
            LOG_MAIN_INFO_AT("Initializing services...");
            auto http = app.GetService<HttpServerService>();
            if (!http) {
                LOG_MAIN_ERROR_AT("Failed to get HttpServer service");
                return false;
            }
            
            return true;
        });

        ApiRouterRegistrar::RegisterAllRoutes();

        // 启动
        app.OnStart([&app]() {
            LOG_MAIN_INFO_AT("Starting services...");

            auto http = app.GetService<HttpServerService>();
            if (http && !http->IsRunning()) {
                if (!http->Start()) {
                    LOG_MAIN_ERROR_AT("Failed to start HttpServer service");
                    return false;
                }
            }

            return true;
        });

        // 停止
        app.OnStop([&app]() {
            LOG_MAIN_INFO_AT("Stopping services...");

            auto http = app.GetService<HttpServerService>();
            if (http && http->IsRunning()) {
                http->Stop();
            }
        });

        // === 7. 运行应用 ===
        LOG_MAIN_INFO_AT("Application running... (Press Ctrl+C to stop)");
        int exit_code = app.Run();

        LOG_MAIN_INFO_AT("Application exited with code: {}", exit_code);        
        log_mgr.FlushAll();  // 确保退出日志输出（内部已包含延迟）
        return exit_code;
        
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Fatal error: {}", e.what());
        LogManager::getInstance().FlushAll();  // 确保错误日志输出
        return 1;
    }
}
