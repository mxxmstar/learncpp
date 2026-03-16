#pragma once
#include "rtp/media.h"
#include "rtp/rtp.h"

#include <functional>
#include <memory>
#include <string>
class MediaSource :public std::enable_shared_from_this<MediaSource> {
public:
    using Ptr = std::shared_ptr<MediaSource>;
    using SendFrameCallback = std::function<bool (MediaChannelId channel_id, RtpPacket pkt)>;
    
    // 禁止拷贝和移动
    MediaSource(const MediaSource&) = delete;
    MediaSource& operator=(const MediaSource&) = delete;
    MediaSource(MediaSource&&) = delete;
    MediaSource& operator=(MediaSource&&) = delete;

    virtual ~MediaSource() = default;

    virtual MediaType GetMediaType() const { 
        return media_type_; 
    }

    virtual std::string GetMediaDescription(uint16_t port=0) = 0;

    virtual std::string GetAttribute()  = 0;

    virtual bool HandleFrame(MediaChannelId channelId, AVFrame frame) = 0;
    
    virtual void SetSendFrameCallback(SendFrameCallback callback) { 
        send_frame_callback_ = std::move(callback); 
    }

    virtual uint32_t GetPayloadType() const { 
        return payload_; 
    }

    virtual uint32_t GetClockRate() const { 
        return clock_rate_; 
    }

protected:
    /// @brief 构造函数
    MediaSource() = default;
    MediaType media_type_ = NONE;
    // 负载类型
    uint32_t  payload_ = 0;
    // 时钟频率
    uint32_t  clock_rate_ = 0;
    SendFrameCallback send_frame_callback_;

    // 允许派生类访问 weak_from_this
    std::weak_ptr<MediaSource> weak_from_this() const {
        return const_cast<MediaSource*>(this)->shared_from_this();
    }
};
