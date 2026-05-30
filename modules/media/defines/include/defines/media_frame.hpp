#pragma once
/// @file media_frame.hpp
/// 视频帧（Frame）定义，包含解码后的图像数据及其元信息。

#include <cstdint>
#include <cstddef>
#include <memory>
#include "i_media_buffer.hpp"
#include "media_packet.hpp"

/// 像素格式
enum class PixelFormat {
    kUnknown = 0,
    kNV12,     ///< YUV420sp (Y + UV 交错)
    kNV21,     ///< YUV420sp (UV 顺序相反)
    kI420,     ///< YUV420p (三个平面)
    kBGR24,    ///< BGR 24 位
    kRGB24,    ///< RGB 24 位
    kGRAY8,    ///< 8 位灰度
};

/// 视频帧：解码后的图像数据及其描述信息
class MediaFrame {
public:
    MediaType    type{MediaType::VIDEO};              ///< 媒体类型（VIDEO/AUDIO）
    PixelFormat pixel_format{PixelFormat::kUnknown}; ///< 像素格式
    int32_t     width{0};                           ///< 图像宽度（像素）
    int32_t     height{0};                          ///< 图像高度（像素）

    /// ===========图像数据相关==========
    int32_t     stride[4] {0};                      ///< 平面行跨度（字节）    
    int32_t     plane_offset[4] {0};                ///< 平面数据偏移（字节）
    int32_t     plane_count{0};                     ///< 平面数量

    /// ===========时间戳相关==========
    int64_t     pts{0};                             ///< 显示时间戳（微秒）
    int64_t     dts{0};                             ///< 解码时间戳（微秒）
    int64_t     duration{0};                        ///< 帧持续时间（微秒）

    /// ===========metadata==========
    bool        keyframe{false};                    ///< 是否为关键帧

    std::shared_ptr<IMediaBuffer> buffer;           ///< packed 图像数据
    BackendHandle backend;                          ///< 后端引擎句柄

public:
    /// 根据宽高和像素格式计算 packed 所需总字节数
    static int32_t CalcVideoSize(int32_t w, int32_t h, PixelFormat fmt) {
        if (w <= 0 || h <= 0) return 0;
        switch (fmt) {
            case PixelFormat::kNV12:
            case PixelFormat::kNV21:
            case PixelFormat::kI420:
                return w * h + w * h / 2;     // Y + UV
            case PixelFormat::kBGR24:
            case PixelFormat::kRGB24:
                return w * h * 3;             // 每像素 3 字节
            case PixelFormat::kGRAY8:
                return w * h;                 // 单通道
            default:
                return 0;
        }
    }

    // /// 获取 Y 平面数据指针
    // uint8_t* DataY() { return buffer ? buffer->Data() : nullptr; }

    // /// 获取 UV 平面数据指针（仅 NV12/NV21，紧接在 Y 平面之后）
    // uint8_t* DataUV() {
    //     if (!buffer || width <= 0 || height <= 0) return nullptr;
    //     return buffer->Data() + static_cast<size_t>(width) * height;
    // }
};
