#pragma once
/// @file media_frame.h
/// C API：可见结构体 media_frame_t，包含 PixelFormat 枚举。

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct media_buffer_t media_buffer_t;

/// 像素格式（与 C++ PixelFormat 一一对应）
typedef enum pixel_format {
    PIXEL_FORMAT_UNKNOWN = 0,
    PIXEL_FORMAT_NV12,     ///< YUV420sp (Y + UV 交错)
    PIXEL_FORMAT_NV21,     ///< YUV420sp (UV 顺序相反)
    PIXEL_FORMAT_YUV420P,     ///< YUV420p (三个平面)
    PIXEL_FORMAT_BGR24,    ///< BGR 24 位
    PIXEL_FORMAT_RGB24,    ///< RGB 24 位
    PIXEL_FORMAT_GRAY8,    ///< 8 位灰度
} pixel_format_t;

/// C 层可见的视频帧结构体（不含 BackendHandle）
typedef struct media_frame_t {
    int32_t         width;          ///< 图像宽度（像素）
    int32_t         height;         ///< 图像高度（像素）
    int32_t         stride_y;       ///< Y 平面行跨度（字节）
    int32_t         stride_uv;      ///< UV 平面行跨度（字节）
    pixel_format_t  pixel_format;   ///< 像素格式
    int64_t         pts;            ///< 显示时间戳（微秒）
    media_buffer_t* buffer;         ///< packed 图像数据
} media_frame_t;

/// 初始化 media_frame_t
void media_frame_init(media_frame_t* frame);
/// 释放内部 buffer 后将所有字段置零
void media_frame_clear(media_frame_t* frame);
/// 根据宽高和像素格式计算 packed 所需总字节数
int32_t media_frame_calc_video_size(int32_t w, int32_t h, pixel_format_t pix_fmt);

#ifdef __cplusplus
}
#endif
