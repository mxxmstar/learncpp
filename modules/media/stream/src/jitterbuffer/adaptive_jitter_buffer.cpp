#include "stream/jitterbuffer/adaptive_jitter_buffer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

AdaptiveJitterBuffer::AdaptiveJitterBuffer()
    : AdaptiveJitterBuffer(Config{}) {
}

AdaptiveJitterBuffer::AdaptiveJitterBuffer(const Config& config)
    : config_(NormalizeConfig(config))
    , incoming_(config_.capacity_packets)
    , current_target_ms_((config_.min_delay_ms + config_.max_delay_ms) / 2.0)
    , avg_delay_ms_(current_target_ms_) {
}

// 基于 EWMA 更新网络延迟的均值和方差，自适应计算目标延迟
// actual_network_delay_ms: 当前包估算的网络延迟
// 返回: 调整后的目标延迟(ms)
double AdaptiveJitterBuffer::UpdateAndGetTargetDelay(double actual_network_delay_ms) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // 1. 计算当前延迟与平均延迟的差值
    double diff = actual_network_delay_ms - avg_delay_ms_;

    // EWMA: avg_new = alpha * avg_old + (1 - alpha) * new_value
    // 2. 基于 EWMA 更新平均延迟和方差
    avg_delay_ms_ += (1.0 - config_.alpha) * diff;
    var_delay_ms_ = config_.alpha * var_delay_ms_ + (1.0 - config_.alpha) * std::abs(diff);

    // 3. 计算目标延迟 = 平均延迟 + 2倍标准差 + 安全余量
    double std_dev = std::sqrt(var_delay_ms_);  // 标准差
    double new_target = avg_delay_ms_ + 2.0 * std_dev + config_.safety_margin_ms;

    // 4. 确保目标延迟在指定范围内
    current_target_ms_ = std::clamp(new_target, config_.min_delay_ms, config_.max_delay_ms);
    return current_target_ms_;
}

double AdaptiveJitterBuffer::GetTargetDelay() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return current_target_ms_;
}

// 生产者：将网络收到的包存入 SPSC 队列，同时估算网络延迟并更新目标
bool AdaptiveJitterBuffer::Push(std::shared_ptr<MediaPacket> packet, Clock::time_point receive_time) {
    if (!packet)
        return false;

    PacketEntry entry;
    entry.packet = std::move(packet);
    entry.receive_time = receive_time;
    entry.receive_wall_ms = WallMilliseconds();
    entry.dts_key = PacketTimestampUs(*entry.packet);
    entry.media_time_ms = PacketTimestampMs(*entry.packet);

    // 根据当前包估算网络延迟，更新 EWMA 统计
    UpdateAndGetTargetDelay(EstimateNetworkDelayMs(entry));

    if (incoming_.push(std::move(entry)))
        return true;

    std::lock_guard<std::mutex> lock(state_mutex_);
    ++dropped_packets_;
    return false;
}

// 消费者：将 SPSC 队列中的包搬运到重排序缓冲区，返回已到期的包
std::shared_ptr<MediaPacket> AdaptiveJitterBuffer::PopReady(Clock::time_point now) {
    DrainIncoming();

    if (reorder_buffer_.empty())
        return nullptr;

    SortReorderBuffer();

    // 检查最老的包是否已达到目标延迟
    auto& oldest = reorder_buffer_.front();
    double residence_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
            now - oldest.receive_time).count());

    if (residence_ms < GetTargetDelay())
        return nullptr;

    auto packet = std::move(oldest.packet);
    int64_t released_dts = oldest.dts_key;
    reorder_buffer_.erase(reorder_buffer_.begin());

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        have_released_dts_ = true;
        last_released_dts_ = released_dts;
    }

    return packet;
}

// 重置所有内部状态到初始值
void AdaptiveJitterBuffer::Reset() {
    incoming_.clear();
    reorder_buffer_.clear();
    reorder_buffer_sorted_ = true;

    std::lock_guard<std::mutex> lock(state_mutex_);
    current_target_ms_ = (config_.min_delay_ms + config_.max_delay_ms) / 2.0;
    avg_delay_ms_ = current_target_ms_;
    var_delay_ms_ = 0.0;
    clock_base_ready_ = false;
    base_receive_ms_ = 0;
    base_media_ms_ = 0;
    have_released_dts_ = false;
    last_released_dts_ = 0;
    dropped_packets_ = 0;
}

std::size_t AdaptiveJitterBuffer::QueuedPackets() const {
    return incoming_.size() + reorder_buffer_.size();
}

std::size_t AdaptiveJitterBuffer::DroppedPackets() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return dropped_packets_;
}

// 校验并归一化配置参数，确保各字段在合法范围内
AdaptiveJitterBuffer::Config AdaptiveJitterBuffer::NormalizeConfig(Config config) {
    if (config.capacity_packets == 0)
        config.capacity_packets = 1;
    if (config.min_delay_ms < 0.0)
        config.min_delay_ms = 0.0;
    if (config.max_delay_ms < config.min_delay_ms)
        config.max_delay_ms = config.min_delay_ms;
    if (config.safety_margin_ms < 0.0)
        config.safety_margin_ms = 0.0;
    if (config.alpha <= 0.0 || config.alpha >= 1.0)
        config.alpha = 0.9;
    return config;
}

int64_t AdaptiveJitterBuffer::ToMilliseconds(Clock::time_point time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch()).count();
}

int64_t AdaptiveJitterBuffer::WallMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// 优先使用 DTS，DTS 无效时回退到 PTS
int64_t AdaptiveJitterBuffer::PacketTimestampUs(const MediaPacket& packet) {
    constexpr int64_t kInvalidTimestamp = std::numeric_limits<int64_t>::min() / 2;
    if (packet.dts > kInvalidTimestamp)
        return packet.dts;
    return packet.pts;
}

int64_t AdaptiveJitterBuffer::PacketTimestampMs(const MediaPacket& packet) {
    return PacketTimestampUs(packet) / 1000;
}

// 估算网络延迟(ms)
// 策略：优先使用 system_clock 时间戳差；若媒体时间戳不可靠则回退到 steady_clock 增量
double AdaptiveJitterBuffer::EstimateNetworkDelayMs(const PacketEntry& entry) {
    constexpr int64_t kEpoch2000Ms = 946684800000LL;     // 2000-01-01 UTC(ms)
    constexpr int64_t kOneDayMs = 24LL * 60LL * 60LL * 1000LL;

    // 如果媒体时间戳合理(>2000年)且与 wall clock 差距在一天以内，直接使用差值
    int64_t wall_diff_ms = entry.receive_wall_ms >= entry.media_time_ms
        ? entry.receive_wall_ms - entry.media_time_ms
        : entry.media_time_ms - entry.receive_wall_ms;
    if (entry.media_time_ms > kEpoch2000Ms &&
        wall_diff_ms < kOneDayMs) {
        return static_cast<double>(entry.receive_wall_ms - entry.media_time_ms);
    }

    // 回退方案：以 steady_clock 为基准，比较相对于基准点的接收增量与媒体增量
    int64_t receive_ms = ToMilliseconds(entry.receive_time);

    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!clock_base_ready_) {
        clock_base_ready_ = true;
        base_receive_ms_ = receive_ms;
        base_media_ms_ = entry.media_time_ms;
        return 0.0;
    }

    int64_t receive_elapsed_ms = receive_ms - base_receive_ms_;
    int64_t media_elapsed_ms = entry.media_time_ms - base_media_ms_;
    return static_cast<double>(receive_elapsed_ms - media_elapsed_ms);
}

// 将 SPSC 队列中所有包搬运到重排序缓冲区，同时丢弃已过期的乱序包
void AdaptiveJitterBuffer::DrainIncoming() {
    PacketEntry entry;
    while (incoming_.pop(entry)) {
        bool drop_late_packet = false;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            // DTS 小于已取出最大 DTS 的包视为过期乱序包，直接丢弃
            drop_late_packet = have_released_dts_ && entry.dts_key < last_released_dts_;
            if (drop_late_packet)
                ++dropped_packets_;
        }

        if (drop_late_packet)
            continue;

        reorder_buffer_.push_back(std::move(entry));
        reorder_buffer_sorted_ = false;
    }
}

// 对重排序缓冲区按 DTS 升序排序（同 DTS 则按接收时间排序）
// 使用 stable_sort 保留同 DTS 包的原始接收顺序
void AdaptiveJitterBuffer::SortReorderBuffer() {
    if (reorder_buffer_sorted_)
        return;

    std::stable_sort(
        reorder_buffer_.begin(),
        reorder_buffer_.end(),
        [](const PacketEntry& lhs, const PacketEntry& rhs) {
            if (lhs.dts_key == rhs.dts_key)
                return lhs.receive_time < rhs.receive_time;
            return lhs.dts_key < rhs.dts_key;
        });
    reorder_buffer_sorted_ = true;
}
