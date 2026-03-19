#pragma once
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <random>
#include <boost/asio.hpp>
#include "rtp/h264source.h"
#include "rtp/media.h"
namespace rtp {

class AsioRtpTransport;
class AsioMediaSession : public std::enable_shared_from_this<AsioMediaSession> {
public:
	using Ptr = std::shared_ptr<AsioMediaSession>;
	using NotifyConnectCallback = std::function<void(MediaSessionId session_id, 
		const std::string& peer_ip, uint16_t peer_port)>;
	using NotifyDisconnectCallback = std::function<void(MediaSessionId session_id, 
		const std::string& peer_ip, uint16_t peer_port)>;

	// static Ptr Create(boost::asio::io_context& io_context, std::string url_suffix = "live");
	explicit AsioMediaSession(boost::asio::io_context& io_context, std::string url_suffix);
	virtual ~AsioMediaSession();

	// 禁止拷贝和移动
	AsioMediaSession(const AsioMediaSession&) = delete;
	AsioMediaSession& operator=(const AsioMediaSession&) = delete;
	AsioMediaSession(AsioMediaSession&&) = delete;
	AsioMediaSession& operator=(AsioMediaSession&&) = delete;


	bool AddSource(MediaChannelId channel_id, std::shared_ptr<MediaSource> source);
	bool RemoveSource(MediaChannelId channel_id);
    bool HasSource(MediaChannelId channel_id);
	std::shared_ptr<MediaSource> GetSource(MediaChannelId channel_id);

	bool HandleFrame(MediaChannelId channel_id, AVFrame frame);

	bool AddClient(boost::asio::ip::tcp::socket& socket, std::shared_ptr<AsioRtpTransport> client);
	void RemoveClient(boost::asio::ip::tcp::socket& socket);
	std::shared_ptr<AsioRtpTransport> GetClient(boost::asio::ip::tcp::socket& socket);
	uint32_t GetClientCount();
	
	/// @brief  启动多播	
	bool StartMulticast();
    
    bool IsMulticastStarted() const;
    std::string GetMulticastIp() const;
    uint16_t GetMulticastPort(MediaChannelId channel_id) const;

    /// @brief 生成SDP消息，用于 DESCRIBE 响应
    /// @param ip server IP 地址
    /// @param session_id RTSP会话ID
    /// @param session_name 会话名称
    /// @return SDP消息
    std::string GenerateSdpMessage(const std::string& ip, const std::string& session_id, const std::string& session_name = "");

	void AddNotifyConnectedCallback(const NotifyConnectCallback& callback);
	void AddNotifyDisconnectedCallback(const NotifyDisconnectCallback& callback);

    MediaSessionId GetSessionId() const;
    const std::string& GetRtspUrlSuffix() const;
    void SetRtspUrlSuffix(const std::string& url_suffix);
private:
    /// @brief 客户端信息
    struct ClientInfo { 
        std::weak_ptr<AsioRtpTransport> client;
        int socket_handle;
        bool is_active;
    };

    /// @brief 分发数据包给所有客户端
    void DispatchToClients(MediaChannelId channel_id, RtpPacket packet);	

	/// @brief IO上下文
	boost::asio::io_context& io_context_;        
	/// @brief 会话ID
	MediaSessionId session_id_ = 0;
	/// @brief Rtsp URL后缀
	std::string url_suffix_;
	/// @brief 缓存的SDP信息
	std::string sdp_;
	/// @brief 媒体源，最多两个 视频+音频
	std::vector<std::shared_ptr<MediaSource>> media_sources_;

	/// @brief 客户端映射表，键为SOCKET句柄，值为AsioRtpTransport的弱指针
	std::map<int, ClientInfo> clients_;

    std::vector<NotifyConnectCallback> notify_connect_callbacks_;
    std::vector<NotifyDisconnectCallback> notify_disconnect_callbacks_;

    /// @brief 客户端映射表的互斥锁
    mutable std::mutex client_mutex_;

    /// @brief 视频帧互斥锁
    mutable std::mutex frame_mutex_;

    /// @brief 多播地址
    std::string multicast_ip_;
    /// @brief 多播端口
    uint16_t multicast_port_[MAX_MEDIA_CHANNEL];
    /// @brief 是否已启动多播
    bool is_multicast_started_;
    /// @brief 是否有新的客户端加入
    std::atomic<bool> has_new_client_;
    /// @brief 会话ID生成
    static inline std::atomic_uint last_session_id_{0}; 
};

class MulticastAddress {
public:
    static MulticastAddress& GetInstance() {
        static MulticastAddress instance;
        return instance;
    }

    std::string GetMulticastIp() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string ip;
		std::random_device rd;
		struct sockaddr_in sa;
        for (int i = 0; i < 10; ++i) {
            // 224.0.1.0 - 224.255.255.255
            uint32_t range = 0xE8FFFFFF - 0xE8000100;
            sa.sin_addr.s_addr = htonl((rd() % range) + 0xE8000100);
            ip = inet_ntoa(sa.sin_addr);
            if (addrs_.find(ip) == addrs_.end()) {
                addrs_.insert(ip);
                break;
            } else {
                ip.clear();
            }
        }
        return ip;
    }

    void Release(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        addrs_.erase(ip);
    }

private:
    MulticastAddress() = default;
    std::mutex mutex_;
	std::unordered_set<std::string> addrs_;
};
}
