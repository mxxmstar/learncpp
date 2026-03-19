#pragma once
#include "rtp/mediasource.h"
#include "rtp/rtp.h"
namespace rtp {

/// @brief H264媒体源，将H264视频帧转换为RTP包
class H264Source : public MediaSource
{
public:
    using Ptr = std::shared_ptr<H264Source>;
    // static Ptr Create(uint32_t frame_rate = 25);
    H264Source(uint32_t frame_rate);
    ~H264Source() override;

    void SetFrameRate(uint32_t frame_rate);
    uint32_t GetFrameRate() const;

    virtual std::string GetMediaDescription(uint16_t port);
    virtual std::string GetAttribute();

    virtual bool HandleFrame(MediaChannelId channelId, AVFrame frame);
    /// @brief 获取当前时间戳的时钟周期    
    static uint32_t GetTimestamp();
private:    
    uint32_t frame_rate_ = 25;
};

}