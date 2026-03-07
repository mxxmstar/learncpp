#include "log/logmanager.h"
#include <iostream>
#include <thread>
#include <chrono>



int main() {
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();

    auto config = LoggerConfig(
        "test",
        spdlog::level::info
    );
    log_manager.RegisterLogger(config);
    LOG_MAIN_INFO("这是一条测试日志");
    LOG_MAIN_ERROR("这是一条错误日志");
    if (nullptr == log_manager.GetLogger("test")) {
        std::cout << "test logger not found" << std::endl;
    } else {
        std::cout << "test logger found" << std::endl;
    }

    log_manager.GetLogger("error")->GetSpdLogger()->error("这是一条错误日志");
    log_manager.GetLogger("main")->GetSpdLogger()->info("这是一条主日志");
    log_manager.GetLogger("main")->GetSpdLogger()->info("===这是一条错误日志, {}", 123);
    LOG_MAIN_ERROR("这是一条错误日志");
    LOG_MAIN_INFO("这是一条主日志");
    LOG_MAIN_WARN("这是一条警告日志");
    LOG_MAIN_CRITICAL("这是一条严重错误日志");
    LOG_MAIN_DEBUG("这是一条调试日志, {}", 123);

    LOG_MAIN_DEBUG_AT("这是一条调试日志, {}", 123);
    LOG_MAIN_INFO_AT("这是一条信息日志, {}", 123);
    LOG_MAIN_WARN_AT("这是一条警告日志, {}", 123);
    LOG_MAIN_CRITICAL_AT("这是一条严重错误日志, {}", 123);
    LOG_ERROR_DEBUG_AT("这是一条错误调试日志, {}", 123);
    LOG_ERROR_INFO_AT("这是一条错误信息日志, {}", 123);
    LOG_ERROR_WARN_AT("这是一条错误警告日志, {}", 123);
    LOG_ERROR_CRITICAL_AT("这是一条错误严重错误日志, {}", 123);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    spdlog::drop_all();
    return 0;
}
