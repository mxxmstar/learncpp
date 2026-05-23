#pragma once

#include "puller/i_puller.hpp"
#include "common/pool/object_pool.hpp"

extern "C" {
#include <libavformat/avformat.h>
}

/// @brief 基于 FFmpeg 的拉流器实现
///
/// 实现 IPuller 纯虚接口，仅负责：
///   - Open()       — 创建 AVFormatContext、查找视频流、缓存 StreamInfo
///   - ReadPacket() — av_read_frame → MediaPacket（零拷贝 FFmpegPacketBuffer）
///   - Close()      — avformat_close_input
///
/// 不负责重连 / watchdog / 状态机 / 统计（均由 StreamSession 管理）。
class FFmpegPuller : public IPuller {
public:
    FFmpegPuller();
    ~FFmpegPuller() override;

    // ==================== IPuller ====================

    bool Open(const std::string& url) override;
    void Close() override;
    bool ReadPacket(std::shared_ptr<MediaPacket>& packet) override;
    StreamInfo GetStreamInfo() const override;
    void SetEventCallback(EventCallback cb) override;

    // ==================== 工具 ====================

    /// @brief AVCodecID → CodecType 映射
    static CodecType MapCodecID(AVCodecID id);

    // ==================== 扩展配置 ====================

    void SetConnectTimeoutMs(int ms);
    void SetReadTimeoutMs(int ms);
    void SetLowLatency(bool enable);
    void SetCredentials(const std::string& username,
                        const std::string& password);

private:
    /// @brief FFmpeg 中断回调上下文
    struct InterruptContext {
        std::atomic<bool> interrupted{false};
        std::chrono::steady_clock::time_point start_time;
        int timeout_ms{5000};
    };

    // ── FFmpeg 资源 ──
    AVFormatContext*   fmt_ctx_{nullptr};     ///< 格式上下文
    int                video_stream_idx_{-1};  ///< 选中视频流索引
    AVCodecParameters* codecpar_{nullptr};     ///< 选中流的编码参数
    InterruptContext   interrupt_ctx_;         ///< 中断回调上下文
    StreamInfo         cached_info_;           ///< 缓存的流信息

    // ── 配置 ──
    int    connect_timeout_ms_{5000};          ///< 连接超时（毫秒）
    int    read_timeout_ms_{10000};            ///< 读超时（毫秒）
    bool   low_latency_{true};                 ///< 低延迟模式
    std::string username_;                     ///< 鉴权用户名
    std::string password_;                     ///< 鉴权密码

    // ── 对象池 ──
    ObjectPool<AVPacket> packet_pool_;         ///< AVPacket 对象池

    // ── 回调 ──
    EventCallback event_cb_;                   ///< 事件回调
};
