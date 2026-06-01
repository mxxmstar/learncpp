#pragma once

#include <string>
#include <unordered_map>

/// @brief 流源全局配置
///
/// 统筹管理 StreamSession 和 IPuller 两层的独立配置。
/// 外部通过 SetStreamSourceConfig() 统一传入，内部自动拆分下发。
struct StreamSourceConfig {
    /// @brief 会话层配置（重连、超时、watchdog）
    struct SessionConfig {
        int connect_timeout_ms    = 5000;   ///< 连接超时（毫秒）
        int read_timeout_ms       = 10000;  ///< 读超时（毫秒）
        int reconnect_interval_ms = 3000;   ///< 重连间隔（毫秒）
        int max_reconnect_count   = -1;     ///< 最大重连次数（-1 无限制）
        int watchdog_interval_ms  = 0;      ///< Watchdog 检测间隔（0 关闭）
        int jitter_buffer_interval_ms = 0;  ///< Jitter buffer 出队间隔（0 关闭）
        int jitter_buffer_capacity_packets = 512;       ///< Jitter buffer 队列容量
        double jitter_buffer_min_delay_ms = 20.0;        ///< 自适应最小缓冲延迟
        double jitter_buffer_max_delay_ms = 200.0;       ///< 自适应最大缓冲延迟
        double jitter_buffer_safety_margin_ms = 10.0;    ///< 突发抖动安全裕度
        double jitter_buffer_alpha = 0.9;                ///< EWMA 平滑因子
    } session;

    /// @brief 拉流器配置（传输层参数）
    struct PullerConfig {
        int    io_timeout_ms     = 5000;            ///< IO 超时（毫秒）
        bool   low_latency       = true;             ///< 低延迟模式
        int    max_delay_ms      = 100;              ///< 最大延迟（毫秒）
        bool   dump_packets      = false;             ///< 调试用包 dump
        std::string rtsp_transport = "udp";            ///< RTSP 传输：udp/tcp/http/...
        bool   rtsp_auto_switch_tcp = false;           ///< UDP 超时后是否允许自动尝试 TCP
        int    rtsp_auto_switch_timeout_ms = 10000;    ///< 自动切换等待时间（毫秒）
        std::string username;                         ///< 鉴权用户名
        std::string password;                         ///< 鉴权密码
        std::unordered_map<std::string, std::string> headers; ///< 自定义 HTTP 头
        int socket_buffer_size = 4 * 1024 * 1024;     ///< 套接字缓冲（字节）
    } puller;
};
