#include <iostream>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
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
    return 0;
}