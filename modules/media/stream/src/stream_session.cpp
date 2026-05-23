#include "stream/session/stream_session.h"

#include <boost/asio/post.hpp>
#include "common/log/logmanager.h"

// ── 辅助 ───────────────────────────────────────────────────────────

inline static const char* StateNameImpl(StreamSession::State s) {
    switch (s) {
        case StreamSession::State::KIDLE:         return "KIDLE";
        case StreamSession::State::KCONNECTING:   return "KCONNECTING";
        case StreamSession::State::KCONNECTED:    return "KCONNECTED";
        case StreamSession::State::KRECONNECTING: return "KRECONNECTING";
        case StreamSession::State::KSTOPPED:      return "KSTOPPED";
        case StreamSession::State::KERROR:        return "KERROR";
        default:                                 return "UNKNOWN";
    }
}

// ── ctor / dtor ────────────────────────────────────────────────────

StreamSession::StreamSession(boost::asio::io_context& io)
    : io_(io)
    , reconnect_timer_(io)
    , watchdog_timer_(io) {
}

StreamSession::~StreamSession() {
    Stop();
}

// ── 配置 ────────────────────────────────────────────────────────────

void StreamSession::SetPuller(std::unique_ptr<IPuller> puller) {
    puller_ = std::move(puller);
}

void StreamSession::SetUrl(const std::string& url) {
    url_ = url;
}

void StreamSession::SetReconnectIntervalMs(int ms) {
    reconnect_interval_ms_ = ms;
}

void StreamSession::SetMaxReconnectCount(int count) {
    max_reconnect_count_ = count;
}

void StreamSession::SetWatchdogIntervalMs(int ms) {
    watchdog_interval_ms_ = ms;
}

// ── 回调 setter ─────────────────────────────────────────────────────

void StreamSession::SetPacketCallback(PacketCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    packet_cb_ = std::move(cb);
}

void StreamSession::SetStreamInfoCallback(StreamInfoCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    streaminfo_cb_ = std::move(cb);
}

void StreamSession::SetStateCallback(StateCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    state_cb_ = std::move(cb);
}

// ── 生命周期 ────────────────────────────────────────────────────────

bool StreamSession::Start() {
    if (!puller_) {
        LOG_MAIN_ERROR_AT("Start() rejected: puller_ is null");
        return false;
    }
    if (url_.empty()) {
        LOG_MAIN_ERROR_AT("Start() rejected: url is empty");
        return false;
    }

    SetState(State::KCONNECTING);

    // 打开拉流器（同步）
    if (!puller_->Open(url_)) {
        SetState(State::KERROR);
        return false;
    }

    // 获取并分发 StreamInfo
    StreamInfo info = puller_->GetStreamInfo();
    if (streaminfo_cb_)
        streaminfo_cb_(info);

    // 标记运行状态
    running_ = true;
    last_read_time_ = std::chrono::steady_clock::now();
    reconnect_count_ = 0;
    stats_ = {};
    async_bytes_received_ = 0;
    async_packets_received_ = 0;

    SetState(State::KCONNECTED);

    // 通过 post 调度读循环到 io_context（由外部线程池驱动）
    boost::asio::post(io_, [self = shared_from_this()]() { self->ReadLoop(); });

    // 启动 Watchdog
    if (watchdog_interval_ms_ > 0)
        StartWatchdog();

    return true;
}

void StreamSession::Stop() {
    State expected = state_.load();
    // 只有 CONNECTED / RECONNECTING / ERROR 需要真正停止
    if (expected != State::KCONNECTED && expected != State::KRECONNECTING &&
        expected != State::KERROR)
        return;   // KIDLE / KCONNECTING / KSTOPPED 无需操作

    // 设置停止标志
    running_ = false;

    // 取消定时器
    boost::system::error_code ec;
    reconnect_timer_.cancel();
    watchdog_timer_.cancel();

    // 关闭拉流器（这会同时中断阻塞中的 av_read_frame，使读循环退出）
    if (puller_)
        puller_->Close();

    SetState(State::KSTOPPED);
}

// ── 内部：连接（同步） ──────────────────────────────────────────────

void StreamSession::Connect() {
    if (!puller_->Open(url_)) {
        LOG_MAIN_ERROR_AT("Reconnect Open failed");
        DoReconnect();
        return;
    }

    // 分发 StreamInfo（重连后可能变化）
    StreamInfo info = puller_->GetStreamInfo();
    if (streaminfo_cb_)
        streaminfo_cb_(info);

    reconnect_count_ = 0;
    last_read_time_ = std::chrono::steady_clock::now();
    SetState(State::KCONNECTED);

    // 重新 post 读循环（io_context 线程池仍在运行）
    running_ = true;
    boost::asio::post(io_, [self = shared_from_this()]() { self->ReadLoop(); });

    // 重启 Watchdog
    if (watchdog_interval_ms_ > 0) {
        boost::system::error_code ec;
        watchdog_timer_.cancel();
        StartWatchdog();
    }
}

// ── 内部：读循环（通过 io_context::post 调度，单次执行） ───────

void StreamSession::ReadLoop() {
    // 每次 handler 入口先检查是否应继续
    if (!running_)
        return;

    // 读一个包（可能阻塞，故在 io_context 多线程环境下需保证其它线程可处理定时器）
    std::shared_ptr<MediaPacket> packet;
    bool ok = puller_->ReadPacket(packet);

    if (!running_)
        return;

    if (ok) {
        // 空 packet = 非目标流跳过，继续下一轮
        if (!packet) {
            boost::asio::post(io_, [self = shared_from_this()]() { self->ReadLoop(); });
            return;
        }

        // 更新统计
        async_bytes_received_  += packet->buffer ? packet->buffer->Size() : 0;
        async_packets_received_++;
        last_read_time_ = std::chrono::steady_clock::now();

        // 分发
        PacketCallback cb;
        {
            std::lock_guard<std::mutex> lock(cb_mutex_);
            cb = packet_cb_;
        }
        if (cb) {
            cb(std::move(packet));
        }

        // 继续下一轮
        boost::asio::post(io_, [self = shared_from_this()]() { self->ReadLoop(); });

    } else {
        // 读取失败：EOF 或错误
        LOG_MAIN_WARN_AT("ReadLoop: ReadPacket failed, reconnecting...");
        DoReconnect();
    }
}

// ── 内部：异步重连 ────────────────────────────────────────────────

void StreamSession::DoReconnect() {
    if (!running_)
        return;

    SetState(State::KRECONNECTING);

    // 关闭旧连接
    if (puller_)
        puller_->Close();

    // 检查重连上限
    if (max_reconnect_count_ >= 0 &&
        reconnect_count_ >= max_reconnect_count_) {
        LOG_MAIN_ERROR_AT("Max reconnect reached ({})", reconnect_count_);
        SetState(State::KERROR);
        return;
    }

    reconnect_count_++;

    LOG_MAIN_INFO_AT("Reconnect attempt {} in {} ms",
                     reconnect_count_, reconnect_interval_ms_);

    // 异步等待后重试
    reconnect_timer_.expires_after(
        std::chrono::milliseconds(reconnect_interval_ms_));
    reconnect_timer_.async_wait(
        [self = shared_from_this()](boost::system::error_code ec) {
            if (ec || !self->running_)
                return;
            self->Connect();
        });
}

// ── 内部：Watchdog ──────────────────────────────────────────────────

void StreamSession::StartWatchdog() {
    watchdog_timer_.expires_after(
        std::chrono::milliseconds(watchdog_interval_ms_));
    watchdog_timer_.async_wait(
        [self = shared_from_this()](boost::system::error_code ec) {
            self->OnWatchdog(ec);
        });
}

void StreamSession::OnWatchdog(const boost::system::error_code& ec) {
    if (ec || !running_)
        return;

    auto now     = std::chrono::steady_clock::now();
    auto idle_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_read_time_).count();

    if (idle_ms > watchdog_interval_ms_) {
        LOG_MAIN_WARN_AT("Watchdog timeout: idle={}ms > interval={}ms",
                         idle_ms, watchdog_interval_ms_);
        // Watchdog 超时触发重连
        DoReconnect();
        return;
    }

    // 继续下一轮检测
    StartWatchdog();
}

// ── 统计 ────────────────────────────────────────────────────────────

StreamSession::Stats StreamSession::GetStats() const {
    return stats_;
}

// ── 状态 ────────────────────────────────────────────────────────────

StreamSession::State StreamSession::GetState() const {
    return state_.load();
}

void StreamSession::SetState(State s) {
    State old = state_.exchange(s);
    if (old == s)
        return;

    LOG_MAIN_INFO_AT("State: {} -> {}", StateNameImpl(old), StateNameImpl(s));

    // 通知状态回调
    StateCallback cb;
    {
        std::lock_guard<std::mutex> lock(cb_mutex_);
        cb = state_cb_;
    }
    if (cb)
        cb(s);
}

const char* StreamSession::StateName(State s) {
    return StateNameImpl(s);
}
