#pragma once
#include "rtp/irtptransport.h"
#include "rtp/rtpsender.h"
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <map>
#include <mutex>
#include <array>

namespace rtp {
/**
 * @brief ASIO 实现的 RTP 传输类
 * @details 该类实现了 IRtpTransport 接口，使用 ASIO 库进行 RTP 传输。
 * @details 只负责 RTP 包头填充和统计,不管理任何 socket    
 * @details 所有发送通过外部传入的 IRtpSender 完成，实现了与传输层的解耦。
 */
class AsioRtpTransport : public IRtpTransport, public std::enable_shared_from_this<AsioRtpTransport> {
public:
    using Ptr = std::shared_ptr<AsioRtpTransport>;
    explicit AsioRtpTransport(boost::asio::io_context& io_context);
    ~AsioRtpTransport() override;

    /// @brief 设置 RTP 发送器
    /// @param sender RTP 发送器    
    void SetSender(IRtpSender::Ptr sender);

    // ========== 禁止拷贝 ==========
    AsioRtpTransport(const AsioRtpTransport&) = delete;
    AsioRtpTransport& operator=(const AsioRtpTransport&) = delete;

    void SetClockRate(MediaChannelId channel_id, uint32_t clock_rate) override;
    void SetPayloadType(MediaChannelId channel_id, uint8_t payload_type) override;
    void SetSsrc(MediaChannelId channel_id, uint32_t ssrc) override;
    void SetSendCallback(SendCallback callback) override;
    bool IsSetup(MediaChannelId channel_id) const override;
    bool IsMulticast() const override;
    std::string GetPeerIp() const override;
    uint16_t GetPeerPort() const override;
    uint32_t GetNativeHandle() const;

    void Start() override;
    void Stop() override;
    bool IsClosed() const override;

    int SendRtpPacket(MediaChannelId channel_id, RtpPacket pkt) override;

    bool SetRtpOverTcp(MediaChannelId channel_id, uint16_t rtp_channel, uint16_t rtcp_channel);

    /// @brief 设置 RTP -over-UDP 传输
    /// @param channel_id 媒体通道 ID
    /// @param rtp_port 对端 RTP 端口号
    /// @param rtcp_port 对端 RTCP 端口号    
    bool SetRtpOverUdp(MediaChannelId channel_id, uint16_t peer_rtp_port, uint16_t peer_rtcp_port);

    /// @brief 配置 RTP -over-Multicast 传输
    /// @param channel_id 媒体通道 ID
    /// @param peer_ip 对端 IP 地址
    /// @param peer_port 对端端口号    
    bool SetRtpOverMulticast(MediaChannelId channel_id, const std::string& peer_ip, uint16_t peer_port);

    std::string GetRtpInfo(const std::string& rtsp_url) const;


private:
    int SendRtp(MediaChannelId channel_id, RtpPacket pkt);
    void FillRtpHeader(MediaChannelId channel_id, RtpPacket& pkt);

    boost::asio::io_context& io_context_;
    IRtpSender::Ptr sender_;    

    bool is_multicast_ = false;
    bool is_closed_ = false;

    MediaChannelInfo media_info_[MAX_MEDIA_CHANNEL];

    SendCallback send_callback_;
    mutable std::mutex mutex_;
};

}