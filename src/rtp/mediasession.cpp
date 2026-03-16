#include "rtp/mediasession.h"
#include "rtp/rtptransport.h"
static const int DEFAULT_MULTICAST_PORT = 5000;

AsioMediaSession::AsioMediaSession(boost::asio::io_context& io_context, std::string url_suffix)
    : io_context_(io_context), url_suffix_(std::move(url_suffix)), session_id_(++last_session_id_)
    , media_sources_(MAX_MEDIA_CHANNEL), has_new_client_(false), is_multicast_started_(false)
{
    for (int i = 0; i < MAX_MEDIA_CHANNEL; i++) {
        multicast_port_[i] = DEFAULT_MULTICAST_PORT + i;
    }
}

// AsioMediaSession::Ptr AsioMediaSession::Create(boost::asio::io_context& io_context, std::string url_suffix)
// {
//     return std::make_shared<AsioMediaSession>(io_context, std::move(url_suffix));
// }

AsioMediaSession::~AsioMediaSession() {
    if (multicast_ip_.empty() == false) {
        MulticastAddress::GetInstance().Release(multicast_ip_);
    }
}

bool AsioMediaSession::AddSource(MediaChannelId channel_id, std::shared_ptr<MediaSource> source) {
    if (channel_id >= MAX_MEDIA_CHANNEL || source == nullptr) {
        return false;
    }

    // 设置回调，使用weak_ptr避免循环引用
    source->SetSendFrameCallback([weak_self = weak_from_this()](MediaChannelId channel_id, RtpPacket pkt) {
        auto self = weak_self.lock();
        if (!self) {
            return true;
        }
        self->DispatchToClients(channel_id, std::move(pkt));
        return true;
    });
    
    media_sources_[channel_id] = source;    // 存储媒体源，用于后续发送帧
    return true;
}

std::shared_ptr<MediaSource> AsioMediaSession::GetSource(MediaChannelId channel_id) {
    if (channel_id >= MAX_MEDIA_CHANNEL) {
        return nullptr;
    }

    return media_sources_[channel_id];
}

bool AsioMediaSession::RemoveSource(MediaChannelId channel_id) {
    if (channel_id >= MAX_MEDIA_CHANNEL) {
        return false;
    }

    media_sources_[channel_id].reset();
    return true;
}
bool AsioMediaSession::HasSource(MediaChannelId channel_id) {
    if (channel_id >= MAX_MEDIA_CHANNEL) {
        return false;
    }

    return media_sources_[channel_id] != nullptr;
}



bool AsioMediaSession::HandleFrame(MediaChannelId channel_id, AVFrame frame) {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (channel_id >= MAX_MEDIA_CHANNEL) {
        return false;
    }

    auto source = media_sources_[channel_id];
    if (source == nullptr) {
        return false;
    }

    return source->HandleFrame(channel_id, frame);
}

bool AsioMediaSession::AddClient(boost::asio::ip::tcp::socket& socket, std::shared_ptr<AsioRtpTransport> client) {
    std::lock_guard<std::mutex> lock(client_mutex_);

    // 使用SOCKET句柄作为客户端ID
    auto client_id = socket.native_handle();
    
    auto it = clients_.find(client_id);
    if (it != clients_.end()) {
        return false;
    }
    
    ClientInfo client_info;
    client_info.client = client;
    client_info.socket_handle = client_id;

    // 存储客户端信息
    clients_[client_id] = std::move(client_info);

    // 通知连接回调
    for (auto& callback : notify_connect_callbacks_) {
        callback(session_id_, client->GetPeerIp(), client->GetPeerPort());
    }
    has_new_client_ = true;
    return true;
}

void AsioMediaSession::RemoveClient(boost::asio::ip::tcp::socket& socket) {
    std::lock_guard<std::mutex> lock(client_mutex_);

    // 使用SOCKET句柄作为客户端ID
    auto client_id = socket.native_handle();

    auto it = clients_.find(client_id);
    if (it == clients_.end()) {
        return;
    }

    auto client = it->second.client.lock();
    if (client != nullptr) {
        // 通知断开回调
        for (auto& callback : notify_disconnect_callbacks_) {
            callback(session_id_, client->GetPeerIp(), client->GetPeerPort());
        }
    }
    // 从map中移除客户端
    clients_.erase(it);
}

std::shared_ptr<AsioRtpTransport> AsioMediaSession::GetClient(boost::asio::ip::tcp::socket& socket) {
    std::lock_guard<std::mutex> lock(client_mutex_);

    // 使用SOCKET句柄作为客户端ID
    auto client_id = socket.native_handle();

    auto it = clients_.find(client_id);
    if (it == clients_.end()) {
        return nullptr;
    }

    return it->second.client.lock();
}

uint32_t AsioMediaSession::GetClientCount() {
    std::lock_guard<std::mutex> lock(client_mutex_);
    uint32_t count = 0;
    for (auto& [client_id, client_info] : clients_) {
        if (!client_info.client.expired() && client_info.is_active) {
            ++count;
        }
    }
    return count;
}

bool AsioMediaSession::StartMulticast() { 
    if (is_multicast_started_) {
        return true;
    }

    multicast_ip_ = MulticastAddress::GetInstance().GetMulticastIp();
    if (multicast_ip_.empty()) {
        return false;
    }

    std::random_device rd;
    // RTP一般使用偶数端口
    multicast_port_[channel_0] = htons(rd() & 0xfffe);
    multicast_port_[channel_1] = htons(rd() & 0xfffe);

    is_multicast_started_ = true;
    return true;
}

bool AsioMediaSession::IsMulticastStarted() const { 
    return is_multicast_started_; 
}

std::string AsioMediaSession::GetMulticastIp() const { 
    return multicast_ip_; 
}

uint16_t AsioMediaSession::GetMulticastPort(MediaChannelId channel_id) const { 
    if (channel_id >= MAX_MEDIA_CHANNEL) {
        return 0;
    }
    return multicast_port_[channel_id]; 
}

std::string AsioMediaSession::GenerateSdpMessage(const std::string& ip, const std::string& session_id, const std::string& session_name) { 
    if (!sdp_.empty()) {
        return sdp_;
    }

    bool has_source = HasSource(channel_0) || HasSource(channel_1);
    if (!has_source) {
        return "";
    }

    char buf[2048] = {0};
    snprintf(buf, sizeof(buf), 
            "v=0\r\n"
            "o=- %s 1 IN IP4 %s\r\n"            
            "t=0 0\r\n",
            "a=control:*\r\n",
            session_id.c_str(),
            ip.c_str()
            );
    if (!session_name.empty()) {
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
            "s=%s\r\n",
            session_name.c_str()
        );
    }

    if (is_multicast_started_) {
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
                "a=type:broadcast\r\n"
                "a=rtcp-unicast: reflection\r\n");
    }

    for (uint32_t i = 0; i < media_sources_.size(); ++i) {
        if (media_sources_[i] != nullptr) {
            if (is_multicast_started_) {
                snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
                        "%s\r\n",
                        media_sources_[i]->GetMediaDescription(multicast_port_[i]).c_str()
                        );
                snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
                        "c=IN IP4 %s/255\r\n",                        
                        multicast_ip_.c_str()
                        );
            }
            else{
                // 非多播模式，port为0
                snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
                        "%s\r\n",
                        media_sources_[i]->GetMediaDescription().c_str()
                        );
            }

            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
                        "%s\r\n",
                        media_sources_[i]->GetAttribute().c_str());

            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
                    "a=control:track%u\r\n", i);
        }
    }

    sdp_ = buf;
    return sdp_;
}

void AsioMediaSession::AddNotifyConnectedCallback(const NotifyConnectCallback& callback) {     
    notify_connect_callbacks_.push_back(callback); 
}

void AsioMediaSession::AddNotifyDisconnectedCallback(const NotifyDisconnectCallback& callback) { 
    notify_disconnect_callbacks_.push_back(callback); 
}

MediaSessionId AsioMediaSession::GetSessionId() const { 
    return session_id_; 
}

const std::string& AsioMediaSession::GetRtspUrlSuffix() const { 
    return url_suffix_; 
}

void AsioMediaSession::SetRtspUrlSuffix(const std::string& url_suffix) { 
    url_suffix_ = url_suffix; 
}

void AsioMediaSession::DispatchToClients(MediaChannelId channel_id, RtpPacket packet) {
    std::lock_guard<std::mutex> lock(client_mutex_);

    // 为每个客户端复制一份RTP包
    // native_handle - RtpPacket
    std::map<int, RtpPacket> client_packets;

    for (auto& [native_handle, client_info] : clients_) {
        if (!client_info.is_active) {
            continue;
        }

        auto client = client_info.client.lock();
        if (!client) {
            continue;
        }

        // 检查是否已经为该客户端创建缓存
        if (client_packets.find(native_handle) == client_packets.end()) {
            RtpPacket client_packet;
            client_packet.type = packet.type;
            client_packet.size = packet.size;
            client_packet.timestamp = packet.timestamp;
            client_packet.last = packet.last;
            memcpy(client_packet.data.get(), packet.data.get(), packet.size);
            client_packets[native_handle] = std::move(client_packet);
        }

        // 异步发送RTP包到客户端                
        if (native_handle >= 0) {
            auto iter = client_packets.find(native_handle);
            if (iter != client_packets.end()) {
                // 线程池中异步发送RTP包
                client->SendRtpPacket(channel_id, iter->second);
                if (is_multicast_started_) {
                    break;  // 多播模式下，只发送一次
                }
            }
        }
    }
}