#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "common/queue/spsc_queue.h"
#include "defines/media_packet.hpp"

// 自适应抖动缓冲器 - 用于消除网络传输中的延迟抖动
// 生产者调用 Push() 存入从网络到达的包，消费者调用 PopReady() 按 DTS 顺序取出
// 内部根据网络延迟变化动态调整等待时间，在延迟与流畅性之间取得平衡
class AdaptiveJitterBuffer {
public:
    using Clock = std::chrono::steady_clock;

    // 配置参数
    struct Config {
        std::size_t capacity_packets{512};   // SPSC 队列容量
        double min_delay_ms{20.0};           // 最小目标延迟(ms)，低于此值可能导致频繁丢包
        double max_delay_ms{200.0};          // 最大目标延迟(ms)，高于此值会引入过大延时
        double safety_margin_ms{10.0};       // 安全余量(ms)，额外缓冲应对突发抖动
        double alpha{0.9};                   // EWMA 平滑系数，越大对抖动变化越不敏感
    };

    AdaptiveJitterBuffer();
    explicit AdaptiveJitterBuffer(const Config& config);

    AdaptiveJitterBuffer(const AdaptiveJitterBuffer&) = delete;
    AdaptiveJitterBuffer& operator=(const AdaptiveJitterBuffer&) = delete;

    // 根据实际网络延迟更新 EWMA 统计，返回新的目标延迟值
    double UpdateAndGetTargetDelay(double actual_network_delay_ms);

    // 获取当前目标延迟值（线程安全）
    double GetTargetDelay() const;

    // 将收到的包推入缓冲队列，返回是否成功（false 表示队列满被丢弃）
    bool Push(std::shared_ptr<MediaPacket> packet,
              Clock::time_point receive_time = Clock::now());

    // 取出等待时间已超过目标延迟的包，按 DTS 升序返回
    std::shared_ptr<MediaPacket> PopReady(Clock::time_point now = Clock::now());

    // 重置所有内部状态和缓冲区
    void Reset();

    std::size_t QueuedPackets() const;    // 当前缓冲中排队总数
    std::size_t DroppedPackets() const;   // 累计丢弃的包数

private:
    // 存储一个包的元信息
    struct PacketEntry {
        std::shared_ptr<MediaPacket> packet;  // 媒体包数据
        Clock::time_point receive_time;       // steady_clock 接收时间（用于计算驻留时间）
        int64_t receive_wall_ms{0};           // system_clock 接收时间（用于网络延迟估算）
        int64_t dts_key{0};                   // 解码时间戳(微秒)，用作排序和丢包判定键
        int64_t media_time_ms{0};             // 媒体时间戳(毫秒)
    };

    static Config NormalizeConfig(Config config);
    static int64_t ToMilliseconds(Clock::time_point time);
    static int64_t WallMilliseconds();
    static int64_t PacketTimestampUs(const MediaPacket& packet);
    static int64_t PacketTimestampMs(const MediaPacket& packet);

    double EstimateNetworkDelayMs(const PacketEntry& entry);
    void DrainIncoming();
    void SortReorderBuffer();

    Config config_;
    common::BoundedSpscQueue<PacketEntry> incoming_;  // 无锁 SPSC 队列，生产端入队

    mutable std::mutex state_mutex_;
    double current_target_ms_{0.0};   // 当前目标延迟(ms)
    double avg_delay_ms_{0.0};        // 网络延迟平均值(EWMA)
    double var_delay_ms_{0.0};        // 网络延迟方差(EWMA)
    bool clock_base_ready_{false};    // 时钟基准是否已建立
    int64_t base_receive_ms_{0};      // 基准接收时间戳(ms)
    int64_t base_media_ms_{0};        // 基准媒体时间戳(ms)
    bool have_released_dts_{false};   // 是否已有包被取出（用于判断乱序丢包）
    int64_t last_released_dts_{0};    // 最后取出的包的 DTS
    std::size_t dropped_packets_{0};  // 累计丢包计数

    std::vector<PacketEntry> reorder_buffer_;  // 重排序缓冲区，消费端处理
    bool reorder_buffer_sorted_{true};         // 重排序缓冲区是否已排序
};
