#pragma once

#include <string>
#include <unordered_map>

enum class TransportType {
    TCP,
    UDP
};

/// @brief 流拉取器配置结构体
struct PullerConfig {
    std::string url;    ///< 拉取地址
    std::string protocol; ///< 协议类型
    // rtsp
    // rtmp
    // httpflv
    // gb28181
    // webrtc

    std::string username; ///< 鉴权用户名
    std::string password; ///< 鉴权密码

    std::unordered_map<std::string, std::string> headers;


    TransportType transport = TransportType::TCP; ///< 传输类型

    int connect_timeout_ms = 5000; ///< 连接超时时间（毫秒）
    int read_timeout_ms = 5000; ///< 读取超时时间（毫秒）
    int io_timeout_ms = 5000; ///< IO超时时间（毫秒）

    bool auto_reconnect = true; ///< 是否自动重连
    int reconnect_interval_ms = 3000; ///< 重连间隔（毫秒）
    int max_reconnect_count = -1; ///< 最大重连次数（-1 表示无限制）

    int socket_buffer_size = 4 * 1024 * 1024; ///< 套接字缓冲区大小（字节）
    int max_packet_queue_size = 1000; ///< 最大包队列大小


    bool low_latency = true; ///< 是否开启低延迟模式
    int max_delay_ms = 100; ///< 最大延迟时间（毫秒）


    bool enable_jitter_buffer = false; ///< 是否开启 Jitter 缓冲区
    int jitter_buffer_ms = 50; ///< Jitter 缓冲区大小（毫秒）
    bool enable_nack = false; ///< 是否开启 NACK 功能
    bool enable_fec = false; ///< 是否开启 FEC 功能


    bool dump_packets = false; ///< 调试
};
