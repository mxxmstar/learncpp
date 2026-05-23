#pragma once

#include <string>
#include <vector>

#include "defines/media_packet.hpp"
#include "common/log/logmanager.h"

/// @brief 流元信息
///
/// 描述一路媒体流的编码参数与元数据。
/// 由 IPuller 在连接成功后构造，传递给 StreamSession → StreamSource。
struct StreamInfo {
    MediaType media_type   = MediaType::UNKNOWN; ///< 媒体类型（视频/音频）
    CodecType codec_type   = CodecType::UNKNOWN; ///< 编码格式（H264/H265/AAC/…）
    int       stream_index = -1;                  ///< 流索引

    int   width       = 0;   ///< 视频宽度
    int   height      = 0;   ///< 视频高度
    float fps         = 0.0; ///< 视频帧率
    int   sample_rate = 0;   ///< 音频采样率
    int   channels    = 0;   ///< 音频通道数

    std::vector<uint8_t> extra_data; ///< 额外数据（如 H.264 SPS/PPS）

    /// @brief 打印流信息到日志
    void Dump() const {
        LOG_MAIN_INFO_AT("StreamInfo: {}",
                         media_type == MediaType::VIDEO ? "VIDEO" : "AUDIO");
        LOG_MAIN_INFO_AT(
            "stream_index: {}, codec_type: {}, width: {}, height: {}, "
            "fps: {}, sample_rate: {}, channels: {}",
            stream_index, static_cast<int>(codec_type),
            width, height, fps, sample_rate, channels);

        if (!extra_data.empty()) {
            std::string hex;
            for (size_t i = 0; i < extra_data.size() && i < 32; ++i) {
                char buf[4];
                std::snprintf(buf, sizeof(buf), "%02x ", extra_data[i]);
                hex += buf;
            }
            LOG_MAIN_INFO_AT("extra_data ({} bytes): {}", extra_data.size(), hex);
        } else {
            LOG_MAIN_INFO_AT("extra_data: empty");
        }
    }
};
