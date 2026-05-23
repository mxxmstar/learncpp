#include "stream/stream_source.h"

#include "common/log/logmanager.h"

// ── ctor / dtor ────────────────────────────────────────────────────

StreamSource::StreamSource(const std::string& stream_id)
    : stream_id_(stream_id) {
}

StreamSource::~StreamSource() {
    Stop();
}

// ── Session ─────────────────────────────────────────────────────────

void StreamSource::SetSession(std::shared_ptr<StreamSession> session) {
    session_ = std::move(session);
}

// ── 生命周期 ────────────────────────────────────────────────────────

bool StreamSource::Start() {
    if (!session_) {
        LOG_MAIN_ERROR_AT("[{}] Start() rejected: session_ is null",
                          stream_id_);
        return false;
    }

    // 应用配置到 session 和 puller
    ApplyConfig();

    // 桥接 session 回调 → source 回调
    {
        std::lock_guard<std::mutex> lock(cb_mutex_);
        session_->SetStreamInfoCallback(
            [self = shared_from_this()](const StreamInfo& info) {
                self->OnStreamInfo(info);
            });
        session_->SetPacketCallback(
            [self = shared_from_this()](std::shared_ptr<MediaPacket> pkt) {
                self->OnPacket(std::move(pkt));
            });
        session_->SetStateCallback(
            [self = shared_from_this()](StreamSession::State state) {
                self->OnSessionState(state);
            });
    }

    // 启动会话
    return session_->Start();
}

void StreamSource::Stop() {
    if (session_)
        session_->Stop();
}

// ── 元数据 ─────────────────────────────────────────────────────────

StreamInfo StreamSource::GetStreamInfo() const {
    return stream_info_;
}

// ── 配置 ────────────────────────────────────────────────────────────

void StreamSource::SetStreamSourceConfig(const StreamSourceConfig& config) {
    config_ = config;
}

// ── 订阅 ────────────────────────────────────────────────────────────

void StreamSource::AddPacketSubscriber(PacketCallback cb) {
    if (!cb)
        return;
    std::lock_guard<std::mutex> lock(subscriber_mutex_);
    packet_subscribers_.push_back(std::move(cb));
}

// ── Session 回调桥接 ───────────────────────────────────────────────

void StreamSource::OnStreamInfo(const StreamInfo& info) {
    stream_info_ = info;
    LOG_MAIN_INFO_AT("[{}] StreamInfo received", stream_id_);
}

void StreamSource::OnPacket(std::shared_ptr<MediaPacket> packet) {
    if (!packet)
        return;

    // 广播给所有订阅者
    std::lock_guard<std::mutex> lock(subscriber_mutex_);
    for (auto& cb : packet_subscribers_) {
        if (cb)
            cb(packet);  // 共享 ptr，所有订阅者共享同一对象
    }
}

void StreamSource::OnSessionState(StreamSession::State state) {
    LOG_MAIN_INFO_AT("[{}] Session state: {}",
                     stream_id_, StreamSession::StateName(state));
    // 预留：后续可在此处处理 decoder/pipeline 生命周期
}

// ── 内部 ────────────────────────────────────────────────────────────

void StreamSource::ApplyConfig() {
    if (!session_)
        return;

    // ── session 层配置 ──
    session_->SetReconnectIntervalMs(config_.session.reconnect_interval_ms);
    session_->SetMaxReconnectCount(config_.session.max_reconnect_count);
    session_->SetWatchdogIntervalMs(config_.session.watchdog_interval_ms);

    // ── puller 层配置（需向下转型） ──
    // FFmpegPuller 提供了 SetXxx 扩展接口，可直接在这里调用
    // 示例（用户根据需要启用）：
    // if (auto* fp = dynamic_cast<FFmpegPuller*>(/*puller指针*/)) {
    //     fp->SetConnectTimeoutMs(config_.puller.io_timeout_ms);
    //     fp->SetLowLatency(config_.puller.low_latency);
    // }
    // 注意：puller 在 session_ 内部，这里无法直接访问。
    // 建议在创建 puller 时直接配置好再注入 session。
}
