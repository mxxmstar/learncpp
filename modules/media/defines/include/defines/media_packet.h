#pragma once
/// @file media_packet.h
/// C API：可见结构体 media_packet_t，包含 MediaType / CodecType 枚举。

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct media_buffer_t media_buffer_t;

/// 媒体流类型（与 C++ MediaType 一一对应）
typedef enum media_type {
    MEDIA_TYPE_UNKNOWN = 0,
    MEDIA_TYPE_VIDEO,
    MEDIA_TYPE_AUDIO,
    MEDIA_TYPE_METADATA,
    MEDIA_TYPE_SEQUENCE_HEADER,
} media_type_t;

/// 编码格式（与 C++ CodecType 一一对应，值参考 FFmpeg AVCodecID）
typedef enum codec_type {
    CODEC_TYPE_UNKNOWN = 0,
    CODEC_TYPE_H264    = 7,
    CODEC_TYPE_H265    = 12,
    CODEC_TYPE_AAC     = 15,
    CODEC_TYPE_G711A   = 7,
    CODEC_TYPE_G711U   = 8,
    CODEC_TYPE_OPUS    = 31,
} codec_type_t;

/// C 层可见的媒体包结构体（不含 BackendHandle，FFI 场景可忽略）
typedef struct media_packet_t {
    media_type_t     media_type;   ///< 媒体流类型
    codec_type_t     codec_type;   ///< 编码格式
    int64_t          pts;          ///< 显示时间戳（微秒）
    int64_t          dts;          ///< 解码时间戳（微秒）
    int              keyframe;     ///< 是否为关键帧（0/1）
    media_buffer_t*  buffer;       ///< 编码数据缓冲区
} media_packet_t;

/// 零初始化 media_packet_t（将所有字段置零，buffer 为 NULL）
void media_packet_init(media_packet_t* pkt);
/// 释放内部 buffer 后将所有字段置零
void media_packet_clear(media_packet_t* pkt);

#ifdef __cplusplus
}
#endif
