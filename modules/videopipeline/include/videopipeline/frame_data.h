#pragma once

#include <cstdint>
#include <vector>
#include <opencv2/opencv.hpp>

/// @brief 视频帧数据结构
/// 用于在流水线各阶段之间传递数据
struct FrameData {
    /// @brief 通道 ID
    int channel_id = -1;
    
    /// @brief 时间戳（微秒）
    int64_t timestamp_us = 0;
    
    /// @brief PTS（解码时间戳，来自 FFmpeg）
    int64_t pts = 0;
    
    /// @brief DTS（显示时间戳，来自 FFmpeg）
    int64_t dts = 0;
    
    /// @brief OpenCV 帧数据（BGR 格式）
    cv::Mat frame;
    
    /// @brief 原始数据包（可选，用于调试或保存）
    std::vector<uint8_t> raw_packet;
    
    /// @brief 帧宽度
    int width = 0;
    
    /// @brief 帧高度
    int height = 0;
    
    /// @brief 像素格式（AVPixelFormat）
    int format = 0;
    
    /// @brief 流 URL
    std::string source_url;
    
    /// @brief 元数据（可扩展）
    std::map<std::string, std::string> metadata;
    
    /// @brief 默认构造函数
    FrameData() = default;
    
    /// @brief 构造函数
    FrameData(int ch_id, int64_t ts, const cv::Mat& frm)
        : channel_id(ch_id)
        , timestamp_us(ts)
        , frame(frm.clone())
        , width(frm.cols)
        , height(frm.rows) {}
    
    /// @brief 移动构造函数
    FrameData(int ch_id, int64_t ts, cv::Mat&& frm)
        : channel_id(ch_id)
        , timestamp_us(ts)
        , frame(std::move(frm))
        , width(this->frame.cols)
        , height(this->frame.rows) {}
    
    /// @brief 获取帧大小（字节）
    size_t getFrameSize() const {
        return frame.total() * frame.elemSize();
    }
    
    /// @brief 检查帧是否有效
    bool isValid() const {
        return !frame.empty() && width > 0 && height > 0;
    }
    
    /// @brief 清空数据
    void clear() {
        frame.release();
        raw_packet.clear();
        metadata.clear();
        width = 0;
        height = 0;
        format = 0;
    }
};

/// @brief 原始数据包（用于 Puller -> Decoder）
struct RawPacketData {
    /// @brief 通道 ID
    int channel_id = -1;
    
    /// @brief 时间戳
    int64_t pts = 0;
    
    /// @brief 数据包
    std::vector<uint8_t> data;
    
    /// @brief 是否为关键帧
    bool is_keyframe = false;
    
    /// @brief 默认构造函数
    RawPacketData() = default;
    
    /// @brief 构造函数
    RawPacketData(int ch_id, int64_t ts, const uint8_t* ptr, size_t size)
        : channel_id(ch_id)
        , pts(ts)
        , data(ptr, ptr + size) {}
    
    /// @brief 移动构造函数
    RawPacketData(int ch_id, int64_t ts, std::vector<uint8_t>&& pkt)
        : channel_id(ch_id)
        , pts(ts)
        , data(std::move(pkt)) {}
};
