#pragma once
#include "rtp/rtp.h"
#include "rtp/media.h"
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <array>

namespace rtp {

class IRtpSender {
public:
    using Ptr = std::shared_ptr<IRtpSender>;
    virtual ~IRtpSender() = default;
    virtual int Send(MediaChannelId channel, const RtpPacket& pkt) = 0;
    ///@brief 获取对端的ip
    ///@note tcp: 返回对端ip udp/multicast: 返回目标ip
    virtual std::string GetPeerIp() const = 0;
    ///@brief 获取对端的rtp端口
    ///@note tcp: 返回对端端口 udp: 返回rtp端口 multicast: 返回组播端口
    virtual uint16_t GetPeerPort() const = 0;
    ///@brief 获取本地端口
    ///@note tcp: 返回rtsp的socket端口 udp/multicast: 返回绑定的本地端口
    virtual uint16_t GetLocalPort() const = 0;    
};

class AsioTcpRtpSender : public IRtpSender {
public:
    /// @brief 创建一个tcp发送器,传入socket引用,保持生命期一致
    explicit AsioTcpRtpSender(boost::asio::ip::tcp::socket& socket);
    int Send(MediaChannelId channel, const RtpPacket& pkt) override;
    /// @brief 获取对端的ip
    std::string GetPeerIp() const override;
    /// @brief 获取对端的端口
    uint16_t GetPeerPort() const override;
    /// @brief 获取本地端口, tcp: 返回0（复用rtsp的socket）
    uint16_t GetLocalPort() const override;
private:
    boost::asio::ip::tcp::socket& socket_;
};

class AsioUdpRtpSender : public IRtpSender {
public:
    AsioUdpRtpSender(boost::asio::io_context& io_context, const std::string& peer_ip,
                     uint16_t peer_rtp_port, uint16_t peer_rtcp_port);
    int Send(MediaChannelId channel, const RtpPacket& pkt) override;
    /// @brief 获取目标ip
    std::string GetPeerIp() const override;
    /// @brief 获取目标rtp端口
    uint16_t GetPeerPort() const override;
    /// @brief 获取本地绑定的rtp端口
    uint16_t GetLocalPort() const override;
private:
    boost::asio::io_context& io_context_;
    std::string peer_ip_;
    uint16_t peer_rtp_port_;
    uint16_t peer_rtcp_port_;
    std::unique_ptr<boost::asio::ip::udp::socket> socket_;
};

class AsioMulticastRtpSender : public IRtpSender {
public:
    AsioMulticastRtpSender(boost::asio::io_context& io_context,
                           const std::string& multicast_ip,
                           uint16_t port);
    int Send(MediaChannelId channel, const RtpPacket& pkt) override;
    /// @brief 获取目标ip
    std::string GetPeerIp() const override;
    /// @brief 获取组播rtp端口
    uint16_t GetPeerPort() const override;
    /// @brief 获取本地绑定的rtp端口
    uint16_t GetLocalPort() const override;
private:
    boost::asio::io_context& io_context_;
    std::string multicast_ip_;
    uint16_t multicast_port_;    
    std::unique_ptr<boost::asio::ip::udp::socket> socket_;
};
}