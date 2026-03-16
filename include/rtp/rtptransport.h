#pragma once
#include "rtp/irtptransport.h"
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <map>
#include <mutex>
#include <array>
/**
 * @brief ASIO 实现的 RTP 传输类
 * @details 该类实现了 IRtpTransport 接口，使用 ASIO 库进行 RTP 传输。
 * @details 管理 RTP 和 RTCP socket，以及 RTP 包的发送。
 */
class AsioRtpTransport : public IRtpTransport, public std::enable_shared_from_this<AsioRtpTransport> {
public:
    using Ptr = std::shared_ptr<AsioRtpTransport>;
    explicit AsioRtpTransport(boost::asio::io_context& io_context,
                              boost::asio::ip::tcp::socket& rtsp_socket);
    ~AsioRtpTransport() override;

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
    // uint32_t GetSessionId() const override;

    void Start() override;
    void Stop() override;
    bool IsClosed() const override;

    int SendRtpPacket(MediaChannelId channel_id, RtpPacket pkt) override;

    bool SetRtpOverTcp(MediaChannelId channel_id, uint16_t rtp_channel, uint16_t rtcp_channel);

    bool SetRtpOverUdp(MediaChannelId channel_id, uint16_t rtp_port, uint16_t rtcp_port);

    bool SetRtpOverMulticast(MediaChannelId channel_id, const std::string& ip, uint16_t port);

    std::string GetRtpInfo(const std::string& rtsp_url) const;


private:
    int SendRtpOverTcp(MediaChannelId channel_id, RtpPacket pkt);
    int SendRtpOverUdp(MediaChannelId channel_id, RtpPacket pkt);
    void FillRtpHeader(MediaChannelId channel_id, RtpPacket& pkt);

    boost::asio::io_context& io_context_;
    ///@brief RTSP 服务器 socket（不拥有所有权）
    boost::asio::ip::tcp::socket& rtsp_socket_;

    // uint32_t session_id_;   ///<@brief RTP ID
    std::string peer_rtsp_ip_;   ///<@brief 对端 RTSP IP
    uint16_t peer_rtsp_port_;     ///<@brief 对端 RTSP 端口

    TransportMode transport_mode_ = TransportMode::RTP_OVER_TCP;
    bool is_multicast_ = false;
    bool is_closed_ = false;

    std::string multicast_ip_;
    uint16_t multicast_port_[MAX_MEDIA_CHANNEL];
    
    ///@brief RTP socket 对象（拥有所有权）
    std::array<std::unique_ptr<boost::asio::ip::udp::socket>, MAX_MEDIA_CHANNEL> rtp_sockets_;
    ///@brief RTCP socket 对象（拥有所有权）
    std::array<std::unique_ptr<boost::asio::ip::udp::socket>, MAX_MEDIA_CHANNEL> rtcp_sockets_;

    // 对端 UDP endpoint 数组
    std::array<boost::asio::ip::udp::endpoint, MAX_MEDIA_CHANNEL> peer_rtp_endpoints_;
    std::array<boost::asio::ip::udp::endpoint, MAX_MEDIA_CHANNEL> peer_rtcp_endpoints_;
    MediaChannelInfo media_info_[MAX_MEDIA_CHANNEL];

    SendCallback send_callback_;
    mutable std::mutex mutex_;
};