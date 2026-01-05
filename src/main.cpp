#include <iostream>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "log/logger.h"
int main() {
    std::cout << "Hello, cross-platform C++!" << std::endl;
#ifdef _WIN32
    std::cout << "Building on Windows" << std::endl;
#elif __linux__
    std::cout << "Building on Linux" << std::endl;
#endif
    auto console = spdlog::stdout_color_mt("console");
    auto err_logger = spdlog::stderr_color_mt("stderr");
    spdlog::get("console")->info("Welcome to spdlog!");

    // 初始化日志系统
    // LogManager::getInstance().Init();
    

    
    std::cout << "日志示例程序执行完成。请检查 logs 目录下的日志文件。" << std::endl;

    return 0;
}