#pragma once
#include <vector>
#include <string>
#include "defines/media_packet.hpp"
#include "common/log/logmanager.h"
struct StreamInfo {    
    MediaType media_type;   ///< 媒体流类型
    CodecType codec_type;   ///< 编码格式
    int stream_index;       ///< 流索引

    int width;              ///< 视频宽度
    int height;             ///< 视频高度
    float fps;              ///< 视频帧率

    int sample_rate;        ///< 音频采样率
    int channels;           ///< 音频通道数

    std::vector<uint8_t> extra_data; ///< 额外数据（如 SPS/PPS）

    void Dump() const {
        LOG_MAIN_INFO_AT("StreamInfo: {}", media_type == MediaType::VIDEO ? "VIDEO" : "AUDIO");
        LOG_MAIN_INFO_AT("stream_index: {}, codec_type: {}, width: {}, height: {}, fps: {}, sample_rate: {}, channels: {}",
                        stream_index, static_cast<int>(codec_type), width, height, fps, sample_rate, channels);
        
        if (!extra_data.empty()) {
            std::string hex_str;
            for (size_t i = 0; i < extra_data.size() && i < 32; ++i) {  // 最多打印前32字节
                char buf[4];
                snprintf(buf, sizeof(buf), "%02x ", extra_data[i]);
                hex_str += buf;
            }
            LOG_MAIN_INFO_AT("extra_data ({} bytes): {}", extra_data.size(), hex_str);
        } else {
            LOG_MAIN_INFO_AT("extra_data: empty");
        }
    }
};
