#include "application/application.h"
#include "config/common_config.h"
// #include "video_pipeline/video_pipeline.h"  // 暂时注释，videopipeline 模块未启用
#include "log/logmanager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>

int main() {
    try {
        // === 第一次初始化：LogManager 简单初始化 ===
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init("./logs", 1);

        LOG_MAIN_INFO_AT("Application starting...");
        log_mgr.FlushAll();  // 确保启动日志输出

        // === Config 加载（现在可以使用日志宏）===
        ConfigManager& config_mgr = ConfigManager::getInstance();
        
        if (!config_mgr.load("tools/config.yaml")) {
            LOG_MAIN_WARN_AT("Failed to load config, using defaults");
        } else {
            LOG_MAIN_INFO_AT("Config loaded successfully");
        }
        log_mgr.FlushAll();  // 确保配置加载日志输出
        
        // 打印配置详情
        config_mgr.dump();
        log_mgr.FlushAll();  // 确保 dump 输出完整（内部已包含延迟）
        
        LOG_MAIN_INFO_AT("Application exiting...");
        log_mgr.FlushAll();  // 确保退出日志输出（内部已包含延迟）
        
        return 0;
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Fatal error: {}", e.what());
        LogManager::getInstance().FlushAll();  // 确保错误日志输出
        return 1;
    }
}
