#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include "defines/media_packet.hpp"
#include "stream_info.hpp"
#include "puller_config.hpp"

/// @brief 拉流器事件枚举
enum class PullerEvent {
    CONNECTED,
    DISCONNECTED,
    RECONNECTING,
    EOF_REACHED,
    ERROR_OCCURED
};

/// @brief 拉流器基类
///
/// 提供一套完整的拉流通用基础设施：
///   - 状态机（IDLE → CONNECTING → CONNECTED ⟷ RECONNECTING → STOPPED）
///   - 基于 boost::asio::io_context 的异步工作循环
///   - 读超时检测
///   - 自动重连（steady_timer 驱动）
///   - 统计信息（steady_timer 每秒异步更新码率/包数）
///   - 三类回调：流信息、媒体包、事件
///
/// 子类只需实现三个纯虚函数：
///   OnConnect()    — 建立传输层连接
///   OnRead()       — 读取一个包并分发
///   OnDisconnect() — 断开传输层连接
class IPuller {
public:
    enum class ReadResult { OK, EOF_, ERROR_ };
    enum class State {
        IDLE,       ///< 默认状态
        OPENED,     ///< 已打开（配置已保存，可 Start）
        CONNECTING, ///< 连接中
        CONNECTED,  ///< 已连接，拉流中
        RECONNECTING, ///< 重连中
        KERROR,     ///< 不可恢复错误
        STOPPED,    ///< 已停止（瞬态）
    };

    struct Stats {
        uint64_t bytes_received{0};
        uint64_t packets_received{0};
        uint64_t packets_dropped{0};
        double   bitrate{0.0};
        uint32_t reconnect_count{0};
    };

    explicit IPuller(boost::asio::io_context& io_ctx);
    virtual ~IPuller();

    virtual bool Open(const PullerConfig& config);
    virtual bool Start();
    virtual void Stop();

    State GetState() const { return state_.load(); }
    Stats GetStats() const;

    using StreamInfoCallback = std::function<void(const StreamInfo&)>;
    using PacketCallback     = std::function<void(std::shared_ptr<MediaPacket>)>;
    using EventCallback      = std::function<void(PullerEvent, const std::string&)>;

    virtual void SetStreamInfoCallback(StreamInfoCallback cb);
    virtual void SetPacketCallback(PacketCallback cb);
    virtual void SetEventCallback(EventCallback cb);
    
protected:    

    virtual bool       OnConnect() = 0;
    virtual ReadResult OnRead()    = 0;
    virtual void       OnDisconnect() = 0;

    void DispatchPacket(std::shared_ptr<MediaPacket> mp);
    void DispatchStreamInfo(const StreamInfo& info);
    void DispatchEvent(PullerEvent ev, const std::string& msg = {});

    void WorkLoop();
    void DoReconnect();

    PullerConfig config_;
    std::atomic<State> state_{State::IDLE};

    boost::asio::io_context& io_ctx_;
    boost::asio::steady_timer reconnect_timer_;

    std::mutex cb_mutex_;
    std::atomic<bool> stopped_{false};

    StreamInfoCallback stream_info_cb_;
    PacketCallback     packet_cb_;
    EventCallback      event_cb_;

    int reconnect_count_{0};
    std::chrono::steady_clock::time_point last_read_time_;

    // 子类通过原子计数器记录字节/包数，定时器每秒汇总到 stats_
    std::atomic<uint64_t> async_bytes_received_{0};
    std::atomic<uint64_t> async_packets_received_{0};

    Stats       stats_;
    Stats       GetStatsSnapshot() const;

private:
    void OnStatsTimer(const boost::system::error_code& ec);

    boost::asio::steady_timer stats_timer_;
};
