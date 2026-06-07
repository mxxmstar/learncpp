#include "pusher/ffmpeg_pusher.hpp"

#include "pusher/ffmpeg_protocol_adapter.hpp"

#include <utility>

FFmpegPusher::FFmpegPusher(PusherConfig config)
    : FFmpegPusher(std::move(config), std::make_unique<FFmpegProtocolAdapter>()) {}

FFmpegPusher::FFmpegPusher(PusherConfig config, std::unique_ptr<IProtocolAdapter> adapter)
    : config_(std::move(config)), adapter_(std::move(adapter)) {}

FFmpegPusher::~FFmpegPusher() {
    Close();
}

bool FFmpegPusher::Connect() {
    if (connected_) {
        return true;
    }

    if (!adapter_) {
        return false;
    }

    connected_ = adapter_->Connect(config_);
    return connected_;
}

bool FFmpegPusher::Send(MediaPacket pkt) {
    if (!connected_ || !adapter_) {
        return false;
    }

    return adapter_->Send(pkt);
}

bool FFmpegPusher::Close() {
    if (!adapter_) {
        connected_ = false;
        return true;
    }

    const bool ok = adapter_->Close();
    connected_ = false;
    return ok;
}

std::unique_ptr<IPusher> IPusher::Create(PusherConfig config) {
    return std::make_unique<FFmpegPusher>(std::move(config));
}
