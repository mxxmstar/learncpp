// @file ffmpeg_packet_buffer.cpp
// FFmpeg AVPacket wrapper implementation
#include "defines/ffmpeg_packet_buffer.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
}

FFmpegPacketBuffer::FFmpegPacketBuffer(AVPacket* pkt) : pkt_(pkt) {}

FFmpegPacketBuffer::~FFmpegPacketBuffer() {
    if (pkt_) av_packet_free(&pkt_);
}

FFmpegPacketBuffer::FFmpegPacketBuffer(FFmpegPacketBuffer&& other) noexcept : pkt_(other.pkt_) {
    other.pkt_ = nullptr;
}

FFmpegPacketBuffer& FFmpegPacketBuffer::operator=(FFmpegPacketBuffer&& other) noexcept {
    if (pkt_) av_packet_free(&pkt_);
    pkt_ = other.pkt_;
    other.pkt_ = nullptr;
    return *this;
}

uint8_t* FFmpegPacketBuffer::Data() {
    return pkt_ ? pkt_->data : nullptr;
}

const uint8_t* FFmpegPacketBuffer::Data() const {
    return pkt_ ? pkt_->data : nullptr;
}

size_t FFmpegPacketBuffer::Size() const {
    return pkt_ ? static_cast<size_t>(pkt_->size) : 0;
}
