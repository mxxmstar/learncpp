#pragma once
#include <functional>
#include <string>
#include "rtp/media.h"
#include "rtp/rtp.h"
namespace rtp {

/// @brief RTP传输接口定义
class IRtpTransport {
public:
    using Prt = std::shared_ptr<IRtpTransport>;
    using SendCallback = std::function<int(MediaChannelId channel_id, RtpPacket pkt)>;

    virtual ~IRtpTransport() = default;

    virtual void SetClockRate(MediaChannelId channel_id, uint32_t clock_rate) = 0;
    virtual void SetPayloadType(MediaChannelId channel_id, uint8_t payload_type) = 0;
    virtual void SetSsrc(MediaChannelId channel_id, uint32_t ssrc) = 0;
    virtual void SetSendCallback(SendCallback callback) = 0;

    /// @brief 判断该通道是否已设置
    virtual bool IsSetup(MediaChannelId channel_id) const = 0;
    /// @brief 是否为多播传输
    virtual bool IsMulticast() const = 0;
    /// @brief 获取对端IP地址
    virtual std::string GetPeerIp() const = 0;
    /// @brief 获取对端端口号
    virtual uint16_t GetPeerPort() const = 0;
    /// @brief 获取会话ID
    //virtual uint32_t GetSessionId() const = 0;

    virtual void Start() = 0;
    virtual void Stop() = 0;    
    virtual bool IsClosed() const = 0;

    /// @brief 发送RTP包
    virtual int SendRtpPacket(MediaChannelId channel_id, RtpPacket pkt) = 0;
};
}