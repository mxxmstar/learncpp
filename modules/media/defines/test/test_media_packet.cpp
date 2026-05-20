// @file test_media_packet.cpp
// 测试 MediaPacket C++ 类和 C API media_packet_t。

#include "defines/media_packet.hpp"
#include "defines/media_packet.h"
#include "defines/media_buffer.h"
#include "common/log/logmanager.h"
#include <cassert>
#include <cstring>

static void test_cpp_packet_defaults() {
    MediaPacket pkt;
    assert(pkt.type == MediaType::kUnknown);
    assert(pkt.codec == CodecType::kUnknown);
    assert(pkt.pts == 0);
    assert(pkt.dts == 0);
    assert(pkt.keyframe == false);
    assert(pkt.buffer == nullptr);
    assert(pkt.backend.type == BackendHandle::kNone);
    assert(pkt.backend.ptr == nullptr);
    LOG_MAIN_INFO("[test] MediaPacket default values ... PASS");
}

static void test_cpp_packet_assign() {
    MediaPacket pkt;
    pkt.type = MediaType::kVideo;
    pkt.codec = CodecType::kH264;
    pkt.pts = 1000;
    pkt.dts = 500;
    pkt.keyframe = true;
    assert(pkt.type == MediaType::kVideo);
    assert(pkt.codec == CodecType::kH264);
    assert(pkt.pts == 1000);
    assert(pkt.dts == 500);
    assert(pkt.keyframe == true);
    LOG_MAIN_INFO("[test] MediaPacket assign fields ... PASS");
}

static void test_c_packet_init_clear() {
    media_packet_t pkt;
    media_packet_init(&pkt);
    assert(pkt.media_type == MEDIA_TYPE_UNKNOWN);
    assert(pkt.codec_type == CODEC_TYPE_UNKNOWN);
    assert(pkt.buffer == nullptr);

    pkt.media_type = MEDIA_TYPE_AUDIO;
    pkt.codec_type = CODEC_TYPE_AAC;
    pkt.pts = 999;

    media_packet_clear(&pkt);
    assert(pkt.media_type == MEDIA_TYPE_UNKNOWN);
    assert(pkt.codec_type == CODEC_TYPE_UNKNOWN);
    assert(pkt.buffer == nullptr);
    LOG_MAIN_INFO("[test] C packet init/clear ... PASS");
}

static void test_c_packet_with_buffer() {
    const uint8_t src[] = {0x00, 0x00, 0x01, 0x67};
    media_packet_t pkt;
    media_packet_init(&pkt);
    pkt.buffer = media_buffer_create(src, 4);
    assert(pkt.buffer != nullptr);
    assert(media_buffer_size(pkt.buffer) == 4);
    media_packet_clear(&pkt);
    assert(pkt.buffer == nullptr);
    LOG_MAIN_INFO("[test] C packet with buffer ... PASS");
}

static void test_c_packet_null() {
    media_packet_init(nullptr);
    media_packet_clear(nullptr);
    LOG_MAIN_INFO("[test] C packet null safe ... PASS");
}

int main() {
    LogManager::getInstance().Init();
    LOG_MAIN_INFO("=== media_packet tests ===");
    test_cpp_packet_defaults();
    test_cpp_packet_assign();
    test_c_packet_init_clear();
    test_c_packet_with_buffer();
    test_c_packet_null();
    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}