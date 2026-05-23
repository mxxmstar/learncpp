#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include "defines/media_packet.hpp"
#include "stream/stream_info.h"
#include "puller/i_puller.hpp"

/// @brief 流会话
///
/// 管理一次拉流连接的生命周期，职责包括：
///   - 连接 / 断开 / 自动重连
///   - 读循环（通过 boost::asio::post 调度到 io_context）
///   - Watchdog 读超时检测（steady_timer）
///   - 码率统计（原子计数器 + 定时汇总）
///
/// 自身不持有 StreamInfo，通过回调向上层（StreamSource）报告。
/// 自身不持有解码器 / pipeline / sink。
///
/// 不持有线程 —— ReadLoop 被 post 到构造时传入的 io_context 上，
/// 由外部线程池或 io_context::run() 驱动。方便配合线程池 / io_context 池统一管理。
class StreamSession :
    public std::enable_shared_from_this<StreamSession> {
public:
    /// @brief 会话状态
    enum class State {
        KIDLE,         ///< 空闲，未启动
        KCONNECTING,   ///< 连接中
        KCONNECTED,    ///< 已连接，正常拉流
        KRECONNECTING, ///< 断线重连中
        KSTOPPED,      ///< 已停止
        KERROR,        ///< 不可恢复错误
    };

    /// @brief 统计快照
    struct Stats {
        uint64_t bytes_received{0};   ///< 累计接收字节
        uint64_t packets_received{0}; ///< 累计接收包数
        double   bitrate{0.0};        ///< 当前码率（kbps）
        uint32_t reconnect_count{0};  ///< 累计重连次数
    };

    /// @brief 构造
    /// @param io 外部 io_context（用于异步定时器）
    explicit StreamSession(boost::asio::io_context& io);

    ~StreamSession();

    // ==================== 生命周期 ====================

    /// @brief 启动会话（打开拉流器 + 拉起读线程）
    /// @return true 启动成功
    bool Start();

    /// @brief 停止会话（关闭拉流器 + 等待读线程退出）
    void Stop();

    // ==================== 配置 ====================

    /// @brief 注入拉流器实例
    void SetPuller(std::unique_ptr<IPuller> puller);

    /// @brief 设置拉流 URL
    void SetUrl(const std::string& url);

    /// @brief 设置重连间隔（毫秒）
    void SetReconnectIntervalMs(int ms);

    /// @brief 设置最大重连次数（-1 无限制）
    void SetMaxReconnectCount(int count);

    /// @brief 设置 Watchdog 间隔（0 关闭）
    void SetWatchdogIntervalMs(int ms);

    // ==================== 回调 ====================

    /// @brief 媒体包回调
    using PacketCallback = std::function<void(std::shared_ptr<MediaPacket>)>;

    /// @brief 流信息回调
    using StreamInfoCallback = std::function<void(const StreamInfo&)>;

    /// @brief 会话状态变更回调
    using StateCallback = std::function<void(State)>;

    void SetPacketCallback(PacketCallback cb);
    void SetStreamInfoCallback(StreamInfoCallback cb);
    void SetStateCallback(StateCallback cb);

    // ==================== 查询 ====================

    /// @brief 获取当前状态
    State GetState() const;

    /// @brief 获取统计快照
    Stats GetStats() const;

    // ==================== 工具 ====================

    /// @brief 转换状态为字符串（公开工具函数）
    static const char* StateName(State s);

private:
    // ==================== 内部流程 ====================

    /// @brief 打开拉流器（由 Start 或 Reconnect 调用）
    void Connect();

    /// @brief 读循环（通过 io_context::post 调度）
    void ReadLoop();

    /// @brief 发起异步重连
    void DoReconnect();

    /// @brief 启动 Watchdog 定时器
    void StartWatchdog();

    /// @brief Watchdog 检测回调
    void OnWatchdog(const boost::system::error_code& ec);

    // ==================== 状态变更 ====================

    /// @brief 安全切换状态并通知回调
    void SetState(State s);

    // ==================== 成员 ====================

    boost::asio::io_context& io_;        ///< 外部 io_context
    std::unique_ptr<IPuller> puller_;    ///< 底层拉流器
    std::string url_;                    ///< 拉流地址

    std::atomic<State> state_{State::KIDLE}; ///< 当前状态
    std::atomic<bool> running_{false};      ///< 读线程运行标志

    boost::asio::steady_timer reconnect_timer_;      ///< 重连延迟定时器
    boost::asio::steady_timer watchdog_timer_;       ///< Watchdog 定时器

    // ── 配置 ──
    int reconnect_interval_ms_{3000};   ///< 重连间隔
    int max_reconnect_count_{-1};       ///< 最大重连次数
    int watchdog_interval_ms_{0};       ///< Watchdog 间隔

    // ── 重连状态 ──
    int reconnect_count_{0};                            ///< 当前连续重连次数
    std::chrono::steady_clock::time_point last_read_time_; ///< 上次成功读时间

    // ── 统计 ──
    std::atomic<uint64_t> async_bytes_received_{0};   ///< 周期内字节（原子）
    std::atomic<uint64_t> async_packets_received_{0}; ///< 周期内包数（原子）
    Stats stats_;                                      ///< 累积统计快照

    // ── 回调 ──
    std::mutex cb_mutex_;                ///< 保护回调 setter，防止启动后又修改了回调函数
    PacketCallback     packet_cb_;       ///< 包分发回调
    StreamInfoCallback streaminfo_cb_;   ///< 流信息回调
    StateCallback      state_cb_;        ///< 状态变更回调
};
