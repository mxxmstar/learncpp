// @file media_packet.cpp
// C API media_packet_init / media_packet_clear 实现�?
#include "defines/media_packet.h"
#include "defines/media_buffer.h"
#include "defines/media_packet.hpp"
#include <cstring>
#include <new>

void media_packet_init(media_packet_t* pkt) {
    if (!pkt) return;
    std::memset(pkt, 0, sizeof(*pkt));
}

void media_packet_clear(media_packet_t* pkt) {
    if (!pkt) return;
    // 先释放内部的 buffer，再清零整个结构�?    media_buffer_destroy(pkt->buffer);
    pkt->buffer = nullptr;
    std::memset(pkt, 0, sizeof(*pkt));
}
