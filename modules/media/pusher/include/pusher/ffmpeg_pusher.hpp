#pragma once

#include <memory>

#include "pusher/i_pusher.hpp"
#include "pusher/protocol_adapter.hpp"

class FFmpegPusher : public IPusher {
public:
    explicit FFmpegPusher(PusherConfig config);
    FFmpegPusher(PusherConfig config, std::unique_ptr<IProtocolAdapter> adapter);
    ~FFmpegPusher() override;

    bool Connect() override;
    bool Send(MediaPacket pkt) override;
    bool Close() override;

private:
    PusherConfig config_;
    std::unique_ptr<IProtocolAdapter> adapter_;
    bool connected_{false};
};
