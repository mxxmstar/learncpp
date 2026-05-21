#include "puller/i_puller.hpp"

#include <boost/asio/post.hpp>
#include "common/log/logmanager.h"
// ── ctor / dtor ────────────────────────────────────────────────────
static inline std::string convertStateToString(IPuller::State state) {
    switch (state) {
        case IPuller::State::IDLE:
            return "IDLE";
        case IPuller::State::OPENED:
            return "OPENED";
        case IPuller::State::CONNECTING:
            return "CONNECTING";
        case IPuller::State::CONNECTED:
            return "CONNECTED";
        case IPuller::State::KERROR:
            return "KERROR";
        case IPuller::State::STOPPED:
            return "STOPPED";
        default:
            return "UNKNOWN";
    }
}


IPuller::IPuller(boost::asio::io_context& io_ctx)
    : io_ctx_(io_ctx)
    , reconnect_timer_(io_ctx)
    , stats_timer_(io_ctx) {
}

IPuller::~IPuller() {
    Stop();
}

// ── Public API ────────────────────────────────────────────────────

bool IPuller::Open(const PullerConfig& config) {
    if (state_.load() != State::IDLE)
        return false;
    config_ = config;
    LOG_MAIN_DEBUG_AT("Open url: {}", config.url);
    state_  = State::OPENED;
    return true;
}

bool IPuller::Start() {
    State expected = State::OPENED;
    if (!state_.compare_exchange_strong(expected, State::CONNECTING)) {
        LOG_MAIN_ERROR_AT("State exchange failed, expected: {}, actual: {}", convertStateToString(expected), convertStateToString(state_.load()));
        return false;
    }

    if (!OnConnect()) {
        state_ = State::KERROR;
        DispatchEvent(PullerEvent::ERROR_OCCURED, "OnConnect failed");
        LOG_MAIN_ERROR_AT("OnConnect failed, state: {}", convertStateToString(state_.load()));
        return false;
    }

    state_ = State::CONNECTED;
    stopped_      = false;
    reconnect_count_ = 0;
    stats_        = {};
    async_bytes_received_ = 0;
    async_packets_received_ = 0;
    last_read_time_ = std::chrono::steady_clock::now();

    DispatchEvent(PullerEvent::CONNECTED);

    // 投递工作循环
    boost::asio::post(io_ctx_, [this]() { WorkLoop(); });

    // 启动异步统计定时器（每秒汇总）
    stats_timer_.expires_after(std::chrono::seconds(1));
    // 再次设置定时器，确保在每次 OnStatsTimer 调用后重新设置
    stats_timer_.async_wait([this](auto ec) { OnStatsTimer(ec); });

    return true;
}

void IPuller::Stop() {
    {
        State expected = State::OPENED;
        if (state_.compare_exchange_strong(expected, State::STOPPED))
            goto do_stop_open;
    }
    {
        State expected = State::CONNECTED;
        if (state_.compare_exchange_strong(expected, State::STOPPED))
            goto do_stop;
    }
    {
        State expected = State::RECONNECTING;
        if (state_.compare_exchange_strong(expected, State::STOPPED))
            goto do_stop;
    }
    {
        State expected = State::KERROR;
        if (state_.compare_exchange_strong(expected, State::STOPPED))
            goto do_stop;
    }
    return;

do_stop_open:
    stopped_ = true;
    state_ = State::IDLE;
    return;

do_stop:
    stopped_ = true;
    reconnect_timer_.cancel();
    stats_timer_.cancel();
    OnDisconnect();

    DispatchEvent(PullerEvent::DISCONNECTED);
    state_ = State::IDLE;
}

// ── 工作循环 ──────────────────────────────────────────────────────

void IPuller::WorkLoop() {
    if (stopped_)
        return;

    // 超时检测
    // TODO: 降低检测的频率，避免频繁触发
    auto now = std::chrono::steady_clock::now();
    auto idle_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_read_time_).count();
    if (config_.read_timeout_ms > 0 && idle_ms > config_.read_timeout_ms) {
        DispatchEvent(PullerEvent::ERROR_OCCURED, "read timeout");
        if (config_.auto_reconnect) {
            DoReconnect();
            return;
        }
        return;
    }

    switch (OnRead()) {
    case ReadResult::OK:
        last_read_time_ = std::chrono::steady_clock::now();
        boost::asio::post(io_ctx_, [this]() { WorkLoop(); });
        break;

    case ReadResult::EOF_:
        DispatchEvent(PullerEvent::EOF_REACHED);
        break;

    case ReadResult::ERROR_:
        DispatchEvent(PullerEvent::ERROR_OCCURED, "read error");
        if (config_.auto_reconnect) {
            DoReconnect();
        }
        break;
    }
}

// ── 异步重连 ──────────────────────────────────────────────────────

void IPuller::DoReconnect() {
    if (stopped_)
        return;

    state_ = State::RECONNECTING;
    DispatchEvent(PullerEvent::RECONNECTING);
    OnDisconnect();

    if (config_.max_reconnect_count >= 0 &&
        reconnect_count_ >= config_.max_reconnect_count) {
        state_ = State::KERROR;
        DispatchEvent(PullerEvent::ERROR_OCCURED, "max reconnect reached");
        return;
    }

    reconnect_count_++;

    reconnect_timer_.expires_after(
        std::chrono::milliseconds(config_.reconnect_interval_ms));
    reconnect_timer_.async_wait([this](boost::system::error_code ec) {
        if (ec || stopped_) return;

        if (OnConnect()) {
            reconnect_count_ = 0;
            last_read_time_ = std::chrono::steady_clock::now();
            state_ = State::CONNECTED;
            DispatchEvent(PullerEvent::CONNECTED);
            boost::asio::post(io_ctx_, [this]() { WorkLoop(); });
        } else {
            DoReconnect();
        }
    });
}

// ── Callbacks ─────────────────────────────────────────────────────

void IPuller::SetStreamInfoCallback(StreamInfoCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    stream_info_cb_ = std::move(cb);
}

void IPuller::SetPacketCallback(PacketCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    packet_cb_ = std::move(cb);
}

void IPuller::SetEventCallback(EventCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    event_cb_ = std::move(cb);
}

void IPuller::DispatchPacket(std::shared_ptr<MediaPacket> mp) {
    if (packet_cb_)
        packet_cb_(std::move(mp));
}

void IPuller::DispatchStreamInfo(const StreamInfo& info) {    
    info.Dump();
    if (stream_info_cb_)
        stream_info_cb_(info);
}

void IPuller::DispatchEvent(PullerEvent ev, const std::string& msg) {
    if (event_cb_)
        event_cb_(ev, msg);
}

// ── Statistics ────────────────────────────────────────────────────

void IPuller::OnStatsTimer(const boost::system::error_code& ec) {
    if (ec || stopped_)
        return;

    // 读取并重置原子计数器
    uint64_t bytes   = async_bytes_received_.exchange(0);
    uint64_t packets = async_packets_received_.exchange(0);

    stats_.bytes_received    += bytes;
    stats_.packets_received  += packets;
    stats_.bitrate            = bytes * 8.0 / 1000.0;   // kbps（采样周期 1s）
    stats_.reconnect_count    = reconnect_count_;

    // 继续下一周期
    stats_timer_.expires_after(std::chrono::seconds(1));
    stats_timer_.async_wait([this](auto ec) { OnStatsTimer(ec); });
}

IPuller::Stats IPuller::GetStats() const {
    return stats_;
}
