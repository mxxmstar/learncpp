// IEncoder 接口和 EncoderConfig 配置定义
// 提供抽象的编码器接口，支持多种编码格式和参数的配置

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "defines/media_frame.hpp"
#include "defines/media_packet.hpp"

using FramePtr = std::shared_ptr<MediaFrame>;
using PacketPtr = std::shared_ptr<MediaPacket>;

// 编码器配置参数
struct EncoderConfig {
    MediaType   media_type{MediaType::VIDEO};    // 媒体类型（视频/音频）
    CodecType   codec_type{CodecType::H264};     // 编码格式（H264/H265/AAC/OPUS）
    PixelFormat pixel_format{PixelFormat::kI420}; // 输入像素格式

    int width{0};          // 视频宽度
    int height{0};         // 视频高度

    int fps_num{25};       // 帧率分子
    int fps_den{1};        // 帧率分母
    int64_t bitrate{2'000'000};  // 目标码率(bps)
    int gop_size{50};      // GOP 大小（关键帧间隔）
    int max_b_frames{0};   // 最大 B 帧数

    // 时间基，如果 <= 0 则自动由 fps_den/fps_num 推导
    int time_base_num{0};
    int time_base_den{0};

    std::string encoder_name;          // 指定的 FFmpeg 编码器名称（如 "libx264"），为空则自动选择
    std::string preset{"ultrafast"};   // 编码器 preset（支持 preset 参数的编码器使用）
    std::string tune{"zerolatency"};   // 编码器 tune 参数（如 zerolatency 降低延迟）
    int crf{-1};                       // CRF 质量控制值（< 0 表示不启用）
    bool global_header{false};         // 是否在 extradata 中存储全局头信息
};

// 编码器抽象接口
class IEncoder {
public:
    virtual ~IEncoder() = default;

    // 打开编码器并应用配置，返回是否成功
    virtual bool Open(const EncoderConfig& cfg) = 0;
    // 编码一帧数据，frame==nullptr 表示刷新（flush）编码器
    virtual bool Encode(FramePtr frame, std::vector<PacketPtr>& packets) = 0;
    // 关闭编码器，释放资源
    virtual void Close() = 0;
};
