//#include "application/application.h"
//#include "common/config/common_config.h"
//// #include "video_pipeline/video_pipeline.h"  // 暂时注释，videopipeline 模块未启用
//#include "common/log/logmanager.h"
//#include <iostream>
//#include <thread>
//#include <chrono>
//#include <boost/asio.hpp>
//
//#include "service/iservice.h"
//#include "service/zlm/zlm_service.h"
//#include "service/http_client/http_client_pool_service.h"
//#include "service/http_server/http_server_service.h"
//#include "api/api_router_registrar.h"
#include <iostream>

int main() {
#if 0
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
         auto config = config_mgr.GetConfig();
         // 批量重新加载所有日志配置
         log_mgr.ReloadFromConfigs(config.logs);

         // === 4. 获取 Application 实例 ===
         auto& app = Application::GetInstance();
        
         // 注册 IService 服务
         auto http_service = HttpServerService::CreateFromAppConfig(config);
         app.RegisterServiceInstance(http_service);
                
          //创建 HttpClientPoolService
         auto http_client_pool_service = std::make_shared<HttpClientPoolService>(config);
         app.RegisterServiceInstance(http_client_pool_service);
         
         // 创建 ZLMService
         auto zlm_service = ZLMService::CreateFromAppConfig(config);
         app.RegisterServiceInstance(zlm_service);
        
         // 注册路由
         ApiRouterRegistrar::RegisterAllRoutes();

         // 初始化验证服务是否存在
         app.OnInit([&app]() {
             LOG_MAIN_INFO_AT("Initializing services...");
             
             // 1. 先初始化 HttpClientPoolService
             auto http_pool = app.GetService<HttpClientPoolService>();
             if (!http_pool) {
                 LOG_MAIN_ERROR_AT("Failed to get HttpClientPool service");
                 return false;
             }
             
             if (!http_pool->Initialize()) {
                 LOG_MAIN_ERROR_AT("Failed to initialize HttpClientPool service");
                 return false;
             }
             
             // 2. 获取 ZLM Client Pool 并注入到 ZLMService
             auto* zlm_client_pool = http_pool->GetZlmClientPool();
             if (!zlm_client_pool) {
                 LOG_MAIN_ERROR_AT("Failed to get ZLM client pool from HttpClientPoolService");
                 return false;
             }
             
             auto zlm_service = app.GetService<ZLMService>();
             if (!zlm_service) {
                 LOG_MAIN_ERROR_AT("Failed to get ZLMService");
                 return false;
             }
             
             zlm_service->SetHttpClientPool(zlm_client_pool);
             LOG_MAIN_INFO_AT("Injected ZLM client pool into ZLMService");
             
             // 3. 初始化 ZLMService
             if (!zlm_service->Initialize()) {
                 LOG_MAIN_ERROR_AT("Failed to initialize ZLMService");
                 return false;
             }
             
             // 4. 初始化 HttpServerService
             auto http = app.GetService<HttpServerService>();
             if (!http) {
                 LOG_MAIN_ERROR_AT("Failed to get HttpServer service");
                 return false;
             }
             
             if (!http->Initialize()) {
                 LOG_MAIN_ERROR_AT("Failed to initialize HttpServer service");
                 return false;
             }
            
             return true;
         });        

         // 启动服务验证
         app.OnStart([&app]() {
             LOG_MAIN_INFO_AT("Starting services...");

             // 1. 启动 HttpClientPoolService
             auto http_client_pool_service = app.GetService<HttpClientPoolService>();
             if (http_client_pool_service && !http_client_pool_service->IsRunning()) {
                 if (!http_client_pool_service->Start()) {
                     LOG_MAIN_ERROR_AT("Failed to start HttpClientPool service");
                     return false;
                 }
             }
             
             // 2. 启动 ZLMService
             auto zlm_service = app.GetService<ZLMService>();
             if (zlm_service && !zlm_service->IsRunning()) {
                 if (!zlm_service->Start()) {
                     LOG_MAIN_ERROR_AT("Failed to start ZLMService");
                     return false;
                 }
             }

             // 3. 启动 HttpServerService
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

             // 1. 停止 HttpServerService
             auto http = app.GetService<HttpServerService>();
             if (http && http->IsRunning()) {
                 http->Stop();
             }
             
             // 2. 停止 ZLMService
             auto zlm_service = app.GetService<ZLMService>();
             if (zlm_service && zlm_service->IsRunning()) {
                 zlm_service->Stop();
             }

             // 3. 停止 HttpClientPoolService
             auto http_client_pool_service = app.GetService<HttpClientPoolService>();
             if (http_client_pool_service && http_client_pool_service->IsRunning()) {
                 http_client_pool_service->Stop();
             }

         });
        
        // === 7. 运行应用 ===
        LOG_MAIN_INFO_AT("Application running... (Press Ctrl+C to stop)");
        int exit_code = app.Run();
        
        LOG_MAIN_INFO_AT("Application exited with code: {}", exit_code);        
        log_mgr.FlushAll();  // 确保退出日志输出（内部已包含延迟）
        // return exit_code;
        return 0;
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Fatal error: {}", e.what());
        LogManager::getInstance().FlushAll();  // 确保错误日志输出
        return 1;
    }
#endif
}
