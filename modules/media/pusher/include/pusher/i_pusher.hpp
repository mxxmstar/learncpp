#pragma once

#include <memory>

#include "defines/media_packet.hpp"
#include "pusher/pusher_config.hpp"

class IPusher {
public:
    virtual ~IPusher() = default;

    virtual bool Connect() = 0;
    virtual bool Send(MediaPacket pkt) = 0;
    virtual bool Close() = 0;

    virtual bool Disconnect() { return Close(); }

    static std::unique_ptr<IPusher> Create(PusherConfig config);
};
