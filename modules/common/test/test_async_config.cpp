#include "common/log/logmanager.h"
#include <iostream>
#include <thread>
#include <chrono>

//void test_async_logger() {
//    std::cout << "\n=== 测试异步日志 ===" << std::endl;
//    
//    LogManager& log_manager = LogManager::getInstance();
//    log_manager.Init("./logs", 1);
//
//    // 创建异步日志器（默认）
//    auto async_config = LoggerConfig("async_test", spdlog::level::debug);
//    async_config.async = true;  // 显式设置为异步
//    async_config.write_to_console = true;
//    log_manager.RegisterLogger(async_config);
//
//    auto logger = log_manager.GetLogger("async_test");
//    if (logger) {
//        for (int i = 0; i < 5; ++i) {
//            logger->GetSpdLogger()->info("异步日志消息 #{}", i);
//        }
//        std::cout << "异步日志已发送，但可能还未输出到控制台..." << std::endl;
//        
//        // 需要flush才能看到所有日志
//        std::this_thread::sleep_for(std::chrono::milliseconds(100));
//        logger->Flush();
//    }
//}
//
//void test_sync_logger() {
//    std::cout << "\n=== 测试同步日志（调试模式）===" << std::endl;
//    
//    LogManager& log_manager = LogManager::getInstance();
//
//    // 创建同步日志器（调试模式）
//    auto sync_config = LoggerConfig("sync_test", spdlog::level::debug);
//    sync_config.async = false;  // 设置为同步
//    sync_config.write_to_console = true;
//    log_manager.RegisterLogger(sync_config);
//
//    auto logger = log_manager.GetLogger("sync_test");
//    if (logger) {
//        for (int i = 0; i < 5; ++i) {
//            logger->GetSpdLogger()->info("同步日志消息 #{}", i);
//            std::cout << "  -> 这条日志会立即输出" << std::endl;
//        }
//        std::cout << "同步日志已立即输出，无需等待或flush" << std::endl;
//    }
//}
//
//void test_config_based_logger() {
//    std::cout << "\n=== 测试从配置加载日志 ===" << std::endl;
//    
//    LogManager& log_manager = LogManager::getInstance();
//    
//    // 模拟从配置文件加载
//    LogConfig cfg;
//    cfg.level = "debug";
//    cfg.dir = "./logs";
//    cfg.rotation = "daily";
//    cfg.console = true;
//    cfg.async = false;  // 调试时设为false
//    
//    auto logger_cfg = LogManager::ConvertToLoggerConfig(cfg, "config_test");
//    log_manager.ReloadFromConfig(logger_cfg);
//    
//    auto logger = log_manager.GetLogger("config_test");
//    if (logger) {
//        logger->GetSpdLogger()->info("从配置创建的{}日志器", cfg.async ? "异步" : "同步");
//    }
//}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "日志模块异步/同步配置测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    //// 测试异步日志
    //test_async_logger();
    //
    //// 测试同步日志
    //test_sync_logger();
    //
    //// 测试配置方式
    //test_config_based_logger();
    //
    //std::cout << "\n========================================" << std::endl;
    //std::cout << "测试完成！" << std::endl;
    //std::cout << "提示：在 config.yaml 中设置 async: false 可启用同步日志方便调试" << std::endl;
    //std::cout << "========================================" << std::endl;
    //
    //// 清理
    //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //LogManager::getInstance().FlushAll();
    //spdlog::drop_all();
    
    return 0;
}
