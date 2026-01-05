#include "log/logmanager.h"
#include <iostream>




int main() {
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();

    auto config = LoggerConfig(
        "test",
        spdlog::level::info
    );
    auto logger = log_manager.RegisterLogger(config);
    logger->info("这是一条测试日志");
    log_manager.GetLogger("test")->info("这是一条测试日志");
    log_manager.GetLogger("test")->error("这是一条错误日志");
    if (nullptr == log_manager.GetLogger("test")) {
        std::cout << "test logger not found" << std::endl;
    } else {
        std::cout << "test logger found" << std::endl;
    }

    log_manager.GetLogger("error")->error("这是一条错误日志");
    log_manager.GetLogger("main")->info("这是一条主日志");
    return 0;
}
