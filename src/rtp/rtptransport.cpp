#include "rtp/rtptransport.h"

AsioRtpTransport::AsioRtpTransport(boost::asio::io_context& io_context, boost::asio::ip::tcp::socket& rtsp_socket)
    : io_context_(io_context), rtsp_socket_(rtsp_socket)
{
    // 获取对端信息
    auto peer_endpoint = rtsp_socket.remote_endpoint();
    peer_rtsp_ip_ = peer_endpoint.address().to_string();
    peer_rtsp_port_ = peer_endpoint.port();

    // 初始化对端 RTP 和 RTCP endpoint
    for (int i = 0; i < MAX_MEDIA_CHANNEL; i++) {        
        peer_rtp_endpoints_[i] = boost::asio::ip::udp::endpoint(peer_endpoint.address(), 0);
        peer_rtcp_endpoints_[i] = boost::asio::ip::udp::endpoint(peer_endpoint.address(), 0);
    }
}

AsioRtpTransport::~AsioRtpTransport() { 
    Stop();
}

void AsioRtpTransport::SetClockRate(MediaChannelId channel_id, uint32_t clock_rate) {
    if (channel_id < 0 || channel_id >= MAX_MEDIA_CHANNEL) {
        return;
    }
    media_info_[channel_id].clock_rate = clock_rate;
}

void AsioRtpTransport::SetPayloadType(MediaChannelId channel_id, uint8_t payload_type) { 
    if (channel_id < 0 || channel_id >= MAX_MEDIA_CHANNEL) {
        return;
    }
    media_info_[channel_id].rtp_header.payload = payload_type & 0x7f;
}

void AsioRtpTransport::SetSsrc(MediaChannelId channel_id, uint32_t ssrc) { 
    if (channel_id < 0 || channel_id >= MAX_MEDIA_CHANNEL) {
        return;
    }
    media_info_[channel_id].rtp_header.ssrc = ssrc;
}

void AsioRtpTransport::SetSendCallback(SendCallback callback) { 
    send_callback_ = std::move(callback);
}

bool AsioRtpTransport::IsSetup(MediaChannelId channel_id) const { 
    return channel_id < MAX_MEDIA_CHANNEL && media_info_[channel_id].is_setup;
}

bool AsioRtpTransport::IsMulticast() const { 
    return is_multicast_;
}

std::string AsioRtpTransport::GetPeerIp() const { 
    return peer_rtsp_ip_;
}

uint16_t AsioRtpTransport::GetPeerPort() const { 
    return peer_rtsp_port_;
}

void AsioRtpTransport::Start() {
    is_closed_ = false;
}

void AsioRtpTransport::Stop() { 
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_closed_) {
        return;
    }
    
    // 关闭所有socket
    for (int i = 0; i < MAX_MEDIA_CHANNEL; i++) {
        if (rtp_sockets_[i] && rtp_sockets_[i]->is_open()) {
            boost::system::error_code ec;
            rtp_sockets_[i]->close(ec);
            rtp_sockets_[i].reset();
        }
        if (rtcp_sockets_[i] && rtcp_sockets_[i]->is_open()) {
            boost::system::error_code ec;
            rtcp_sockets_[i]->close(ec);
            rtcp_sockets_[i].reset();
        }
    }
    is_closed_ = true;
}

bool AsioRtpTransport::IsClosed() const { 
    return is_closed_;
}

int AsioRtpTransport::SendRtpPacket(MediaChannelId channel_id, RtpPacket pkt) { 
    if (is_closed_ || send_callback_ == nullptr) {
        return -1;
    }

    if (transport_mode_ == TransportMode::RTP_OVER_TCP) {
        return SendRtpOverTcp(channel_id, std::move(pkt));
    }

    if (transport_mode_ == TransportMode::RTP_OVER_UDP) { 
        return SendRtpOverUdp(channel_id, std::move(pkt));
    }

    return -1;
}

bool AsioRtpTransport::SetRtpOverTcp(MediaChannelId channel_id, uint16_t rtp_channel, uint16_t rtcp_channel) { 
    std::lock_guard<std::mutex> lock(mutex_);
    if (channel_id < 0 || channel_id >= MAX_MEDIA_CHANNEL) {
        return false;
    }
    // 保存通道号
    media_info_[channel_id].local_rtp_channel = rtp_channel;
    media_info_[channel_id].local_rtcp_channel = rtcp_channel;
    media_info_[channel_id].is_setup = true;

    transport_mode_ = TransportMode::RTP_OVER_TCP;
    return true;
}

bool AsioRtpTransport::SetRtpOverUdp(MediaChannelId channel_id, uint16_t rtp_port, uint16_t rtcp_port) { 
    std::lock_guard<std::mutex> lock(mutex_);
    if (channel_id < 0 || channel_id >= MAX_MEDIA_CHANNEL) {
        return false;
    }

    try {
        transport_mode_ = TransportMode::RTP_OVER_UDP;

        // 创建socket
        rtp_sockets_[channel_id] = std::make_unique<boost::asio::ip::udp::socket>(io_context_);
        rtcp_sockets_[channel_id] = std::make_unique<boost::asio::ip::udp::socket>(io_context_);
        rtp_sockets_[channel_id]->open(boost::asio::ip::udp::v4());
        rtcp_sockets_[channel_id]->open(boost::asio::ip::udp::v4());

        // 绑定到端口 0，让操作系统自动分配
        rtp_sockets_[channel_id]->bind(
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));
        rtcp_sockets_[channel_id]->bind(
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));

        // 设置对端地址
        auto peer_endpoint = rtsp_socket_.remote_endpoint();
        peer_rtp_endpoints_[channel_id] = boost::asio::ip::udp::endpoint(
            peer_endpoint.address(), rtp_port);
        
        peer_rtcp_endpoints_[channel_id] = boost::asio::ip::udp::endpoint(
            peer_endpoint.address(), rtcp_port);
        
        
         // 获取操作系统分配的端口，保存到media_info_
        boost::system::error_code ec;
        auto local_rtp_endpoint = rtp_sockets_[channel_id]->local_endpoint(ec);
        auto local_rtcp_endpoint = rtcp_sockets_[channel_id]->local_endpoint(ec);
        if (ec) {
            return false;
        }
        
        media_info_[channel_id].local_rtp_port = local_rtp_endpoint.port();
        media_info_[channel_id].local_rtcp_port = local_rtcp_endpoint.port();
        media_info_[channel_id].is_setup = true;        
    } catch (std::exception& e) {
        return false;
    }
    return true;
}

bool AsioRtpTransport::SetRtpOverMulticast(MediaChannelId channel_id, const std::string& ip, uint16_t port) { 
    std::lock_guard<std::mutex> lock(mutex_);
    if (channel_id < 0 || channel_id >= MAX_MEDIA_CHANNEL) {
        return false;
    }
    try { 
        transport_mode_ = TransportMode::RTP_OVER_MULTICAST;
        is_multicast_ = true;
        multicast_ip_ = ip;
        multicast_port_[channel_id] = port;

        // 创建 RTP socket
        rtp_sockets_[channel_id] = std::make_unique<boost::asio::ip::udp::socket>(io_context_);        
        rtp_sockets_[channel_id]->open(boost::asio::ip::udp::v4());        

        boost::asio::ip::address multicast_address = boost::asio::ip::make_address(ip);
        rtp_sockets_[channel_id]->set_option(boost::asio::ip::multicast::join_group(multicast_address));


        // 创建 RTCP socket（可选，根据实际需求）
        rtcp_sockets_[channel_id] = std::make_unique<boost::asio::ip::udp::socket>(io_context_);
        rtcp_sockets_[channel_id]->open(boost::asio::ip::udp::v4());
        rtcp_sockets_[channel_id]->bind(
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port + 1));
        
        peer_rtp_endpoints_[channel_id] = boost::asio::ip::udp::endpoint(
            multicast_address, port);

        // ✅ 保存本地端口（多播模式下通常不需要）
        // boost::system::error_code ec;
        // auto local_endpoint = rtp_sockets_[channel_id]->local_endpoint(ec);
        // if (!ec) {
        //     media_info_[channel_id].local_rtp_port = local_endpoint.port();
        // }
        media_info_[channel_id].is_setup = true;
    } catch (std::exception& e) { 
        return false;
    }
    return true;
}

std::string AsioRtpTransport::GetRtpInfo(const std::string& rtsp_url) const { 
    std::string rtp_info;

    auto time_point = std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now());
    auto ts = time_point.time_since_epoch().count();

    int num = 0;
    for (int i = 0; i < MAX_MEDIA_CHANNEL; ++i) { 
        if (media_info_[i].is_setup) {
            if (num != 0) {
                rtp_info += ",";
            }

            // 根据 clock_rate 计算时间戳
            uint32_t rtptime = static_cast<uint32_t>(ts * media_info_[i].clock_rate / 1000);
            char buf[256] = {0};
            snprintf(buf, sizeof(buf), 
                    "url=%s/track%d;seq=0;rtptime=%u",
                    rtsp_url.c_str(), i, rtptime);
            rtp_info += buf;
            ++num;
        }
    }
    return rtp_info;    
}

int AsioRtpTransport::SendRtpOverTcp(MediaChannelId channel_id, RtpPacket pkt) { 
    FillRtpHeader(channel_id, pkt);
    
    uint8_t* ptr = pkt.data.get();
    // TCP需要添加4字节帧头部进行数据包定界
    ptr[0] = '$';   // 帧头标识, RTP/RTCP 数据帧
    ptr[1] = static_cast<uint8_t>(channel_id);  // 通道号
    ptr[2] = static_cast<uint8_t>(((pkt.size - 4) & 0xFF00) >> 8);  // 数据长度高8位
    ptr[3] = static_cast<uint8_t>((pkt.size - 4) & 0xFF);  // 数据长度低8位

    boost::system::error_code ec;
    boost::asio::write(rtsp_socket_, boost::asio::buffer(ptr, pkt.size), ec);
    if (ec) {
        return -1;
    }

    media_info_[channel_id].packet_count++;
    media_info_[channel_id].octet_count += pkt.size;

    return 0;
}

int AsioRtpTransport::SendRtpOverUdp(MediaChannelId channel_id, RtpPacket pkt) { 
    if (channel_id >= MAX_MEDIA_CHANNEL) {
        return -1;
    }

    if (rtp_sockets_[channel_id] == nullptr || rtp_sockets_[channel_id]->is_open() == false) {
        return -1;
    }

    FillRtpHeader(channel_id, pkt);

    uint8_t* ptr = pkt.data.get();
    // UDP不需要添加4字节帧头部进行数据包定界,但是空出4字节进行对齐
    auto size = pkt.size - 4;

    boost::system::error_code ec;
    rtp_sockets_[channel_id]->send_to(
        boost::asio::buffer(ptr, size),
        peer_rtp_endpoints_[channel_id],
        0, ec);

    if (ec) {
        return -1;
    }

    media_info_[channel_id].packet_count++;
    media_info_[channel_id].octet_count += size;

    return 0;
}

void AsioRtpTransport::FillRtpHeader(MediaChannelId channel_id, RtpPacket& pkt) { 
    auto& info = media_info_[channel_id];

    // 跳过4字节对齐
    RtpHeader* header = reinterpret_cast<RtpHeader*>(pkt.data.get() + 4);
    header->version = RTP_VERSION;
    header->padding = 0;
    header->extension = 0;
    header->csrc = 0;
    header->marker = pkt.last ? 1 : 0;
    header->payload = info.rtp_header.payload;
    header->seq = htons(info.rtp_header.seq++);
    header->ts = htonl(pkt.timestamp);
    header->ssrc = htonl(info.rtp_header.ssrc);
}

