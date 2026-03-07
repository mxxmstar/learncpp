#include "rtsp_log.h"

RtspLogger& RtspLogger::GetInstance() {
    static RtspLogger instance;
    return instance;
}

void RtspLogger::Init() {
    LoggerConfig config("rtsp", spdlog::level::debug);
    config.write_to_console = true;
    config.write_to_main_log = true;
    LogManager::getInstance().RegisterLogger(config);
    LogManager::getInstance().Init();
}

std::shared_ptr<Logger> RtspLogger::GetLogger() {
    return LogManager::getInstance().GetLogger("rtsp");
}
