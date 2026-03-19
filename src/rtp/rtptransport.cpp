#include "rtp/rtptransport.h"
namespace rtp{
AsioRtpTransport::AsioRtpTransport(boost::asio::io_context& io_context)
    : io_context_(io_context)
{
}

AsioRtpTransport::~AsioRtpTransport() { 
    Stop();
}

void AsioRtpTransport::SetSender(IRtpSender::Ptr sender) {
    sender_ = std::move(sender);    
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
    return sender_->GetPeerIp();
}

uint16_t AsioRtpTransport::GetPeerPort() const { 
    return sender_->GetPeerPort();
}

uint32_t AsioRtpTransport::GetNativeHandle() const {
    return 0;
}

void AsioRtpTransport::Start() {
    is_closed_ = false;
}

void AsioRtpTransport::Stop() { 
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_closed_) {
        return;
    }
    is_closed_ = true;
}

bool AsioRtpTransport::IsClosed() const { 
    return is_closed_;
}

int AsioRtpTransport::SendRtpPacket(MediaChannelId channel_id, RtpPacket pkt) { 
    if (is_closed_ || sender_ == nullptr) {
        return -1;
    }
    
    int ret = SendRtp(channel_id, std::move(pkt));
    if (ret < 0) {
        return -1;
    }
    
    media_info_[channel_id].packet_count++;
    media_info_[channel_id].octet_count += pkt.size;
    
    return 0;
}

bool AsioRtpTransport::SetRtpOverTcp(MediaChannelId channel_id, uint16_t rtp_channel, uint16_t rtcp_channel) { 
    std::lock_guard<std::mutex> lock(mutex_);
    if (channel_id < 0 || channel_id >= MAX_MEDIA_CHANNEL) {
        return false;
    }
    
    auto& info = media_info_[channel_id];
    info.SetTcpMode(rtp_channel, rtcp_channel);
    return true;
}

bool AsioRtpTransport::SetRtpOverUdp(MediaChannelId channel_id, uint16_t peer_rtp_port, uint16_t peer_rtcp_port) { 
    std::lock_guard<std::mutex> lock(mutex_);
    if (channel_id < 0 || channel_id >= MAX_MEDIA_CHANNEL) {
        return false;
    }
        
    auto& info = media_info_[channel_id];
    info.SetUdpMode(peer_rtp_port, peer_rtcp_port, sender_->GetLocalPort(), sender_->GetLocalPort() + 1);    
        
    return true;
}

bool AsioRtpTransport::SetRtpOverMulticast(MediaChannelId channel_id, const std::string& peer_ip, uint16_t peer_port) { 
    std::lock_guard<std::mutex> lock(mutex_);
    if (channel_id < 0 || channel_id >= MAX_MEDIA_CHANNEL) {
        return false;
    }
        
    is_multicast_ = true;    
    
    auto& info = media_info_[channel_id];
    info.SetMulticastMode(peer_ip, peer_port);
    
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

int AsioRtpTransport::SendRtp(MediaChannelId channel_id, RtpPacket pkt) { 
    if (!sender_) {
        return -1;
    }

    FillRtpHeader(channel_id, pkt);
    
    int ret = sender_->Send(channel_id, pkt);
    if (ret < 0) {
        return -1;
    }

    return 0;
}

void AsioRtpTransport::FillRtpHeader(MediaChannelId channel_id, RtpPacket& pkt) { 
    auto& info = media_info_[channel_id];

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
}