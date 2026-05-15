#pragma once

#include <functional>
#include <cstdint>
#include <memory>
#include "common/detection_result.h"

// FFmpeg 内存管理函数（前向声明）
extern "C" {
    void av_free(void *ptr);
}

/// @brief 通用视频帧结构
struct VideoFrame {
    uint8_t* data[4];      // 图像数据指针（YUV/RGB 等）
    int linesize[4];       // 每行字节数
    int width;             // 宽度
    int height;            // 高度
    int format;            // 像素格式（AVPixelFormat）
    uint64_t pts;           // 时间戳
    bool inference_done = false;     // 是否完成推理
    bool osd_done = false;           // 是否完成 OSD 渲染
    DetectionResult result;          // 检测结果
    VideoFrame() : width(0), height(0), format(-1), pts(0) {
        for (int i = 0; i < 4; ++i) {
            data[i] = nullptr;
            linesize[i] = 0;
        }
    }
    
    // 移动构造函数
    VideoFrame(VideoFrame&& other) noexcept {
        for (int i = 0; i < 4; ++i) {
            data[i] = other.data[i];
            linesize[i] = other.linesize[i];
            other.data[i] = nullptr;
            other.linesize[i] = 0;
        }
        width = other.width;
        height = other.height;
        format = other.format;
        pts = other.pts;
        inference_done = other.inference_done;
        osd_done = other.osd_done;
        result = std::move(other.result);
        other.width = 0;
        other.height = 0;
        other.format = -1;
        other.pts = 0;
        other.inference_done = false;
        other.osd_done = false;
    }
    
    // 移动赋值运算符
    VideoFrame& operator=(VideoFrame&& other) noexcept {
        if (this != &other) {
            for (int i = 0; i < 4; ++i) {
                if (data[i]) {
                    av_free(data[i]);
                    data[i] = nullptr;
                    linesize[i] = 0;
                }
            }
            for (int i = 0; i < 4; ++i) {
                data[i] = other.data[i];
                linesize[i] = other.linesize[i];
                other.data[i] = nullptr;
                other.linesize[i] = 0;
            }
            width = other.width;
            height = other.height;
            format = other.format;
            pts = other.pts;
            inference_done = other.inference_done;
            osd_done = other.osd_done;
            result = std::move(other.result);
            other.width = 0;
            other.height = 0;
            other.format = -1;
            other.pts = 0;
            other.inference_done = false;
            other.osd_done = false;
        }
        return *this;
    }
    
    // 析构函数：释放所有平面内存
    ~VideoFrame() {
        for (int i = 0; i < 4; ++i) {
            if (data[i]) {
                av_free(data[i]);
                data[i] = nullptr;
            }
        }
    }
    
    // 禁止拷贝
    VideoFrame(const VideoFrame&) = delete;
    VideoFrame& operator=(const VideoFrame&) = delete;
};

/// @brief 解码器接口
class IDecoder {
public:
    /// @brief 帧回调函数类型（传递通用帧）
    using FrameCallback = std::function<void(VideoFrame&& frame)>;
    
    virtual ~IDecoder() = default;
    
    /// @brief 打开解码器
    /// @param extradata 额外数据（编解码器参数）
    /// @param extradata_size 额外数据大小
    /// @param codec_id 编解码器 ID
    /// @return true 成功，false 失败
    virtual bool Open(const uint8_t* extradata, int extradata_size, int codec_id) = 0;
    
    /// @brief 解码数据包
    /// @param packet 数据包
    /// @param size 数据包大小
    /// @param pts 显示时间戳
    /// @param cb 帧回调函数
    virtual void Decode(const uint8_t* packet, int size, int64_t pts, FrameCallback cb) = 0;
    
    /// @brief 关闭解码器
    virtual void Close() = 0;
};
