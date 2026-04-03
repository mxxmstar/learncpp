#pragma once

#include <opencv2/opencv.hpp>
#include <functional>
#include <cstdint>

/// @brief 解码器接口
class IDecoder {
public:
    /// @brief 帧回调函数类型
    using FrameCallback = std::function<void(cv::Mat&& frame, int64_t pts)>;
    
    virtual ~IDecoder() = default;
    
    /// @brief 打开解码器
    /// @param extradata 额外数据（编解码器参数）
    /// @param extradata_size 额外数据大小
    /// @param codec_id 编解码器 ID
    /// @return true 成功，false 失败
    virtual bool open(const uint8_t* extradata, int extradata_size, int codec_id) = 0;
    
    /// @brief 解码数据包
    /// @param packet 数据包
    /// @param size 数据包大小
    /// @param pts 显示时间戳
    /// @param cb 帧回调函数
    virtual void decode(const uint8_t* packet, int size, int64_t pts, FrameCallback cb) = 0;
    
    /// @brief 关闭解码器
    virtual void close() = 0;
};
