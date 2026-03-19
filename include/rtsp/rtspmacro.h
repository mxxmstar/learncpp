#pragma once
#include "log/logmanager.h"

#define LOG_RTSP_DEBUG(...) do { \
    LogManager::getInstance().GetLogger("rtsp")->GetSpdLogger()->debug(__VA_ARGS__); \
} while(0)

#define LOG_RTSP_INFO(...) do { \
    LogManager::getInstance().GetLogger("rtsp")->GetSpdLogger()->info(__VA_ARGS__); \
} while(0)

#define LOG_RTSP_WARN(...) do { \
    LogManager::getInstance().GetLogger("rtsp")->GetSpdLogger()->warn(__VA_ARGS__); \
} while(0)

#define LOG_RTSP_ERROR(...) do { \
    LogManager::getInstance().GetLogger("rtsp")->GetSpdLogger()->error(__VA_ARGS__); \
} while(0)

#define LOG_RTSP_CRITICAL(...) do { \
    LogManager::getInstance().GetLogger("rtsp")->GetSpdLogger()->critical(__VA_ARGS__); \
} while(0)

#define LOG_RTSP_DEBUG_FL(filename, line, ...) do { \
    LogManager::getInstance().GetLogger("rtsp")->GetSpdLogger()->debug("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_RTSP_INFO_FL(filename, line, ...) do { \
    LogManager::getInstance().GetLogger("rtsp")->GetSpdLogger()->info("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_RTSP_WARN_FL(filename, line, ...) do { \
    LogManager::getInstance().GetLogger("rtsp")->GetSpdLogger()->warn("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_RTSP_ERROR_FL(filename, line, ...) do { \
    LogManager::getInstance().GetLogger("rtsp")->GetSpdLogger()->error("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_RTSP_CRITICAL_FL(filename, line, ...) do { \
    LogManager::getInstance().GetLogger("rtsp")->GetSpdLogger()->critical("[{}#{}]{}", extract_filename(filename), line, spdlog::fmt_lib::format(__VA_ARGS__)); \
} while(0)

#define LOG_RTSP_DEBUG_AT(...) LOG_RTSP_DEBUG_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_RTSP_INFO_AT(...)  LOG_RTSP_INFO_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_RTSP_WARN_AT(...)  LOG_RTSP_WARN_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_RTSP_ERROR_AT(...) LOG_RTSP_ERROR_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_RTSP_CRITICAL_AT(...) LOG_RTSP_CRITICAL_FL(__FILE__, __LINE__, __VA_ARGS__)
