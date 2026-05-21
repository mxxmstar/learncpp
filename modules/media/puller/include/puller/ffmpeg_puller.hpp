#pragma once

#include "i_puller.hpp"

extern "C" {
#include <libavformat/avformat.h>
}

/// @brief 基于 FFmpeg 的拉流器实现
///
/// 继承 IPuller，仅实现三个传输原语：
///   - OnConnect()    — 打开 FFmpeg 上下文、查找视频流、发送 StreamInfo
///   - OnRead()       — av_read_frame → MediaPacket → DispatchPacket
///   - OnDisconnect() — avformat_close_input
///
/// 重连、超时、状态机、统计等通用逻辑全部由 IPuller 基类管理。
class FFmpegPuller : public IPuller {
public:
    /// @brief 构造
    /// @param io_ctx 外部传入的 io_context
    explicit FFmpegPuller(boost::asio::io_context& io_ctx);

    ~FFmpegPuller() override;

    /// @brief AVCodecID → CodecType 映射（公开工具函数）
    static CodecType MapCodecID(AVCodecID id);

protected:
    bool       OnConnect() override;
    ReadResult OnRead()    override;
    void       OnDisconnect() override;

private:
    /// @brief 中断回调上下文
    struct InterruptContext {
        std::atomic<bool> interrupted{false};
        std::chrono::steady_clock::time_point start_time;
        int timeout_ms;
    };

    AVFormatContext*    fmt_ctx_{nullptr};          ///< FFmpeg 格式上下文
    int                 video_stream_idx_{-1};       ///< 选中视频流的索引
    AVCodecParameters*  codecpar_{nullptr};          ///< 选中视频流的编码参数
    InterruptContext    interrupt_ctx_;              ///< 中断回调上下文
};
