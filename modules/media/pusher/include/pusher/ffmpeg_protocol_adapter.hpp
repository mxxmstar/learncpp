#pragma once

#include "pusher/protocol_adapter.hpp"

struct AVFormatContext;
struct AVPacket;

class FFmpegProtocolAdapter : public IProtocolAdapter {
public:
    FFmpegProtocolAdapter() = default;
    ~FFmpegProtocolAdapter() override;

    FFmpegProtocolAdapter(const FFmpegProtocolAdapter&) = delete;
    FFmpegProtocolAdapter& operator=(const FFmpegProtocolAdapter&) = delete;

    bool Connect(const PusherConfig& config) override;
    bool Send(const MediaPacket& pkt) override;
    bool Close() override;

    static int MapCodecType(CodecType type);

private:
    bool BuildStream();
    bool BuildPacket(const MediaPacket& pkt, AVPacket* out) const;
    bool FreeContext(bool write_trailer);

    PusherConfig config_;
    AVFormatContext* fmt_ctx_{nullptr};
    int stream_index_{-1};
    bool header_written_{false};
};
