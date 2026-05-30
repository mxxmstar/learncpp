#pragma once
/// @file media_packet.hpp
/// 媒体包（Packet）定义，包含编码数据及其元信息。

#include <cstdint>
#include <cstddef>
#include <memory>
#include "i_media_buffer.hpp"

/// 媒体流类型
enum class MediaType {    
    VIDEO,            ///< 视频
    AUDIO,            ///< 音频
    UNKNOWN = 0,      ///< 未知
};

/// 编码格式（值参考 FFmpeg 的 AVCodecID）
enum class CodecType : int {
    UNKNOWN = 0,
    H264    = 7,
    H265    = 12,
    AAC     = 15,
    G711A   = 7,
    G711U   = 8,
    OPUS    = 31,
};

/// 后端引擎句柄，用于传递特定引擎的内部对象指针
struct BackendHandle {
    enum Type {
        NONE = 0,     ///< 无后端
        FFMPEG,       ///< FFmpeg AVPacket / AVFrame
        OPENH264,     ///< OpenH264 编码器
        WEBRTC,       ///< WebRTC 内部缓冲区
    };
    Type type{NONE};  ///< 后端类型
    void* ptr{nullptr};///< 后端内部对象指针
};

/// 媒体包：一个编码帧或编码帧分片的数据及其描述信息
class MediaPacket {
public:
    MediaType  type{MediaType::UNKNOWN};         ///< 媒体流类型
    CodecType  codec{CodecType::UNKNOWN};        ///< 编码格式
    int64_t    pts{0};                            ///< 显示时间戳（微秒）
    int64_t    dts{0};                            ///< 解码时间戳（微秒）
    bool       keyframe{false};                   ///< 是否为关键帧
    std::shared_ptr<IMediaBuffer> buffer;          ///< 编码数据载荷
    BackendHandle backend;                         ///< 后端引擎句柄
};
