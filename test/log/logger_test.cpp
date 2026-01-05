#include "log/logger.h"
#include <iostream>


int main() {
    try {
        LoggerConfig config;
        config.name = "test_logger";
        config.log_dir = "./log";
        config.level = spdlog::level::debug;
        config.write_to_console = true;
        config.write_to_main_log = true;
        config.is_json = false;

        Logger logger(config);

        auto spd_logger = logger.GetLogger();
        std::cout << "Logger initialized" << std::endl;

        spd_logger->info("Test message");
        spd_logger->debug("Debug message");
        spd_logger->error("Error message");
        spd_logger->warn("Warning message");
        std::cout << "Logging complete" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    }
}