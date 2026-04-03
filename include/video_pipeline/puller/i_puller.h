#pragma once

#include <string>
#include <functional>
#include <cstdint>

/// @brief 拉流器接口
class IPuller {
public:
    /// @brief 数据回调函数类型
    using FrameCallback = std::function<void(const uint8_t* data, int size, int64_t pts)>;
    
    virtual ~IPuller() = default;
    
    /// @brief 启动拉流
    /// @param url 流地址（RTSP/RTMP/HTTP-FLV）
    /// @param cb 数据回调函数
    /// @return true 成功，false 失败
    virtual bool start(const std::string& url, FrameCallback cb) = 0;
    
    /// @brief 停止拉流
    virtual void stop() = 0;
    
    /// @brief 是否正在运行
    virtual bool isRunning() const = 0;
};
