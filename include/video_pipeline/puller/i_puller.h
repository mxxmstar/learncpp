#pragma once

#include <string>
#include <functional>
#include <cstdint>

/// @brief 拉流器接口
class IPuller {
public:
    /// @brief 序列头回调函数类型
    using SequenceHeaderCallback = std::function<void(int codec_id, const uint8_t* data, int size)>;
    
    /// @brief 数据回调函数类型
    using FrameCallback = std::function<void(const uint8_t* data, int size, int64_t pts)>;
    
    virtual ~IPuller() = default;
    
    /// @brief 启动拉流
    /// @param url 流地址（RTSP/RTMP/HTTP-FLV）
    /// @param seq_cb 序列头回调函数
    /// @param frame_cb 数据回调函数
    /// @return true 成功，false 失败
    virtual bool start(const std::string& url, 
                      SequenceHeaderCallback seq_cb,
                      FrameCallback frame_cb) = 0;
    
    /// @brief 停止拉流
    virtual void stop() = 0;
    
    /// @brief 是否正在运行
    virtual bool isRunning() const = 0;
};