#pragma once

#include "defines/media_packet.hpp"
#include "pusher/pusher_config.hpp"

class IProtocolAdapter {
public:
    virtual ~IProtocolAdapter() = default;

    virtual bool Connect(const PusherConfig& config) = 0;
    virtual bool Send(const MediaPacket& pkt) = 0;
    virtual bool Close() = 0;
};
