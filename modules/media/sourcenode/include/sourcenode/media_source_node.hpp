#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include <boost/asio/io_context.hpp>

#include "common/log/logmanager.h"
#include "common/runtime/node.h"
#include "decoder/i_decoder.hpp"
#include "defines/media_frame.hpp"
#include "defines/media_packet.hpp"
#include "puller/i_puller.hpp"
#include "stream/session/source_config.h"
#include "stream/session/stream_session.h"
#include "stream/stream_source.h"

/// @brief 拉流源节点 — Pull → Demux → Decode 一体化 SourceNode
///
/// 将拉流器 (IPuller) 与解码器 (IDecoder) 封装为一个 ISourceNode，
/// 内部自动完成：
///   1. Pull   — 底层拉流（如 FFmpegPuller）
///   2. Demux  — 流信息获取与解码器初始化
///   3. Decode — 逐包解码，输出 MediaFrame
///
/// 可直接注册到 Runtime / AsioRuntime 中作为 Source 节点使用。
///
/// 用法：
/// @code
///   boost::asio::io_context io;
///   auto node = std::make_shared<MediaSourceNode>(
///       io, "rtsp://...",
///       std::make_unique<FFmpegPuller>(),
///       std::make_unique<FFmpegDecoder>());
///
///   runtime.AddSource("camera", node);
///   runtime.Start();
/// @endcode
class MediaSourceNode : public common::runtime::ISourceNode<std::shared_ptr<MediaFrame>> {
public:
    using Frame = std::shared_ptr<MediaFrame>;

    /// @brief 构造
    /// @param io         Boost.Asio io_context（驱动 StreamSession 的异步读循环）
    /// @param url        拉流地址
    /// @param puller     拉流器实例（接管所有权）
    /// @param decoder    解码器实例（接管所有权）
    /// @param stream_id  流标识（为空时自动用 url 代替）
    MediaSourceNode(boost::asio::io_context& io,
                     std::string url,
                     std::unique_ptr<IPuller> puller,
                     std::unique_ptr<IDecoder> decoder,
                     std::string stream_id = {})
        : stream_id_(stream_id.empty() ? url : std::move(stream_id))
        , url_(std::move(url))
        , io_(io)
        , puller_(std::move(puller))
        , decoder_(std::move(decoder)) {
    }

    ~MediaSourceNode() override { Stop(); }

    MediaSourceNode(const MediaSourceNode&) = delete;
    MediaSourceNode& operator=(const MediaSourceNode&) = delete;

    // ==================== ISourceNode ====================

    void Start() override {
        if (started_.exchange(true))
            return;

        session_ = std::make_shared<StreamSession>(io_);
        session_->SetPuller(std::move(puller_));
        session_->SetUrl(url_);

        if (reconnect_interval_ms_ >= 0)
            session_->SetReconnectIntervalMs(reconnect_interval_ms_);
        if (max_reconnect_count_ >= 0)
            session_->SetMaxReconnectCount(max_reconnect_count_);
        if (watchdog_interval_ms_ >= 0)
            session_->SetWatchdogIntervalMs(watchdog_interval_ms_);

        // 设置流信息回调，用于解码器初始化
        session_->SetStreamInfoCallback(
            [this](const StreamInfo& info) { OnStreamInfo(info); });

        // 将 Packet 传递给解码器进行解码
        session_->SetPacketCallback(
            [this](std::shared_ptr<MediaPacket> pkt) { OnPacket(std::move(pkt)); });

        source_ = std::make_shared<StreamSource>(stream_id_);
        source_->SetSession(session_);
        if (config_set_)
            source_->SetStreamSourceConfig(config_);

        source_->Start();
    }

    void Stop() override {
        if (!started_.exchange(false))
            return;

        if (source_)
            source_->Stop();
        if (decoder_)
            decoder_->Close();

        source_.reset();
        session_.reset();
    }

    // ==================== 配置 ====================

    void SetStreamSourceConfig(const StreamSourceConfig& config) {
        config_ = config;
        config_set_ = true;
    }

    void SetReconnectIntervalMs(int ms) {
        reconnect_interval_ms_ = ms;
        if (session_)
            session_->SetReconnectIntervalMs(ms);
    }

    void SetMaxReconnectCount(int count) {
        max_reconnect_count_ = count;
        if (session_)
            session_->SetMaxReconnectCount(count);
    }

    void SetWatchdogIntervalMs(int ms) {
        watchdog_interval_ms_ = ms;
        if (session_)
            session_->SetWatchdogIntervalMs(ms);
    }

    // ==================== 查询 ====================

    StreamInfo GetStreamInfo() const {
        return source_ ? source_->GetStreamInfo() : StreamInfo{};
    }

private:
    void OnStreamInfo(const StreamInfo& info) {
        decoder_->SetFrameCallback(
            [this](std::shared_ptr<MediaFrame> frame) {
                if (frame)
                    Emit(std::move(frame));
            });

        if (!decoder_->Open(info)) {
            LOG_MAIN_ERROR_AT("MediaSourceNode[{}]: decoder Open failed", stream_id_);
        }
    }

    void OnPacket(std::shared_ptr<MediaPacket> packet) {
        if (packet && packet->buffer) {
            decoder_->Decode(std::move(packet));
        }
    }

    std::string stream_id_;
    std::string url_;
    boost::asio::io_context& io_;
    std::unique_ptr<IPuller> puller_;
    std::unique_ptr<IDecoder> decoder_;

    std::shared_ptr<StreamSource> source_;
    std::shared_ptr<StreamSession> session_;

    std::atomic<bool> started_{false};
    bool config_set_{false};

    int reconnect_interval_ms_{-1}; ///< 重连间隔，-1 表示不启用重连
    int max_reconnect_count_{-1};   ///< 最大尝试次数，-1 表示无限重试
    int watchdog_interval_ms_{-1};   ///< 监控器间隔，-1 表示不启用监控
    StreamSourceConfig config_{};
};
