#pragma once
#include <cstdint>
#include <string>
#include <ctime>

/// @brief 获取当前 Unix 时间戳（秒）
inline int64_t GetCurrentTimestamp() {
    return static_cast<int64_t>(std::time(nullptr));
}

/// @brief 将 Unix 时间戳转换为可读字符串（用于日志和显示）
inline std::string TimestampToString(int64_t timestamp) {
    if (timestamp == 0) {
        return "";
    }
    
    std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm tm_buf;
    localtime_s(&tm_buf, &time);
    
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return std::string(buffer);
}

/// @brief 将 ISO 8601 字符串转换为 Unix 时间戳
inline int64_t StringToTimestamp(const std::string& iso_string) {
    if (iso_string.empty()) {
        return 0;
    }
    
    // 简单解析 ISO 8601 格式: YYYY-MM-DDTHH:MM:SS
    std::tm tm = {};
    sscanf_s(iso_string.c_str(), "%d-%d-%dT%d:%d:%d",
             &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
             &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    tm.tm_isdst = -1;
    
    return static_cast<int64_t>(mktime(&tm));
}
