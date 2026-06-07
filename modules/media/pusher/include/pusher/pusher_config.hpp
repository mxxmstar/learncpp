#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "defines/media_packet.hpp"

struct PusherConfig {
    std::string url;
    std::string format_name;
    std::string rtsp_transport{"tcp"};

    MediaType media_type{MediaType::VIDEO};
    CodecType codec_type{CodecType::H264};

    int width{0};
    int height{0};
    int sample_rate{0};
    int channels{0};

    int time_base_num{1};
    int time_base_den{90000};

    std::vector<uint8_t> extra_data;

    bool IsValid() const {
        if (url.empty() || codec_type == CodecType::UNKNOWN ||
            time_base_num <= 0 || time_base_den <= 0) {
            return false;
        }

        if (media_type == MediaType::VIDEO) {
            return width > 0 && height > 0;
        }

        if (media_type == MediaType::AUDIO) {
            return sample_rate > 0 && channels > 0;
        }

        return false;
    }
};
