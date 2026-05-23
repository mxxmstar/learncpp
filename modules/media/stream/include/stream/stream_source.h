#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "defines/media_packet.hpp"
#include "stream/stream_info.h"
#include "stream/session/stream_session.h"
#include "stream/session/source_config.h"

/// @brief 媒体源
///
/// 一个 StreamSource 对应一路流。
///
/// 职责：
///   - 持有 StreamSession，管理拉流生命周期
///   - 缓存 StreamInfo（元数据）
///   - 向多个订阅者分发 MediaPacket
///
/// 后续扩展（设计预留）：
///   - decoder 管理
///   - pipeline 管理
///   - sink 管理
class StreamSource :
    public std::enable_shared_from_this<StreamSource> {
public:
    /// @brief 构造
    /// @param stream_id 全局唯一流标识
    explicit StreamSource(const std::string& stream_id);

    ~StreamSource();

    // ==================== Session ====================

    /// @brief 注入会话实例
    /// @param session 外部创建的 StreamSession（应已完成 SetPuller/SetUrl 等配置）
    void SetSession(std::shared_ptr<StreamSession> session);

    // ==================== 生命周期 ====================

    /// @brief 启动拉流（通过 session 打开连接、拉起读线程）
    /// @return true 启动成功
    bool Start();

    /// @brief 停止拉流
    void Stop();

    // ==================== 元数据 ====================

    /// @brief 获取流信息（连接成功后有效）
    StreamInfo GetStreamInfo() const;

    // ==================== 配置 ====================

    /// @brief 设置全局配置（自动拆分下发至 session 与 puller）
    void SetStreamSourceConfig(const StreamSourceConfig& config);

    // ==================== 订阅 ====================

    /// @brief 包订阅回调
    using PacketCallback = std::function<void(std::shared_ptr<MediaPacket>)>;

    /// @brief 添加一个包订阅者
    /// @param cb 每次读取到包时被调用
    void AddPacketSubscriber(PacketCallback cb);

private:
    // ==================== Session 回调桥接 ====================

    void OnStreamInfo(const StreamInfo& info);
    void OnPacket(std::shared_ptr<MediaPacket> packet);
    void OnSessionState(StreamSession::State state);

    // ==================== 内部 ====================

    /// @brief 将 config_ 中的 session/puller 配置应用到对应组件
    void ApplyConfig();

    std::string stream_id_;                         ///< 流标识
    std::shared_ptr<StreamSession> session_;        ///< 拉流会话
    StreamInfo stream_info_;                        ///< 缓存的流元信息
    StreamSourceConfig config_;                     ///< 全局配置

    // ── 订阅者 ──
    std::mutex subscriber_mutex_;                           ///< 保护 subscribers
    std::vector<PacketCallback> packet_subscribers_;        ///< 包订阅者列表

    // ── 保护回调 setter（session_setter 互斥） ──
    std::mutex cb_mutex_;
};
