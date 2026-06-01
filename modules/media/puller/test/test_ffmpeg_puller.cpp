/// @file test_ffmpeg_puller.cpp
/// FFmpegPuller 单元测试：MapCodecID、Open/Close/ReadPacket 原语。

#include "puller/ffmpeg_puller.hpp"
#include "common/log/logmanager.h"

#include <boost/asio/io_context.hpp>

#include <cassert>
#include <chrono>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
}

// ── 辅助 ───────────────────────────────────────────────────────────

#define TEST(name) \
    do { LOG_MAIN_INFO_AT("[test] {} ...", name); } while (0)

#define PASS() \
    LOG_MAIN_INFO_AT("  PASS")

// ── 测试 MapCodecID ───────────────────────────────────────────────

static void test_map_codec_id_h264() {
    TEST("MapCodecID(AV_CODEC_ID_H264) == CodecType::H264");
    assert(FFmpegPuller::MapCodecID(AV_CODEC_ID_H264) == CodecType::H264);
    PASS();
}

static void test_map_codec_id_hevc() {
    TEST("MapCodecID(AV_CODEC_ID_HEVC) == CodecType::H265");
    assert(FFmpegPuller::MapCodecID(AV_CODEC_ID_HEVC) == CodecType::H265);
    PASS();
}

static void test_map_codec_id_aac() {
    TEST("MapCodecID(AV_CODEC_ID_AAC) == CodecType::AAC");
    assert(FFmpegPuller::MapCodecID(AV_CODEC_ID_AAC) == CodecType::AAC);
    PASS();
}

static void test_map_codec_id_opus() {
    TEST("MapCodecID(AV_CODEC_ID_OPUS) == CodecType::OPUS");
    assert(FFmpegPuller::MapCodecID(AV_CODEC_ID_OPUS) == CodecType::OPUS);
    PASS();
}

static void test_map_codec_id_unknown() {
    TEST("MapCodecID(unknown) == CodecType::UNKNOWN");
    assert(FFmpegPuller::MapCodecID(AV_CODEC_ID_MPEG4) == CodecType::UNKNOWN);
    assert(FFmpegPuller::MapCodecID(AV_CODEC_ID_VP9) == CodecType::UNKNOWN);
    assert(FFmpegPuller::MapCodecID(static_cast<AVCodecID>(-1)) == CodecType::UNKNOWN);
    PASS();
}

// ── 测试构造 / 析构 ───────────────────────────────────────────────

static void test_construct() {
    TEST("Build FFmpegPuller, verify IPuller inheritance");
    FFmpegPuller puller;
    IPuller* base = dynamic_cast<IPuller*>(&puller);
    assert(base != nullptr);
    PASS();
}

// ── 测试 Open ──────────────────────────────────────────────────────

static void test_open_empty_url() {
    TEST("Open(empty url) returns false");
    FFmpegPuller puller;
    assert(!puller.Open(""));
    PASS();
}

static void test_open_bogus_url() {
    TEST("Open(bogus url) returns false");
    FFmpegPuller puller;
    puller.SetConnectTimeoutMs(100);
    puller.SetReadTimeoutMs(100);
    assert(!puller.Open("rtsp://192.0.2.1:55555/live/test"));
    PASS();
}

// ── 测试 Close ─────────────────────────────────────────────────────

static void test_close_unopened() {
    TEST("Close() without Open");
    FFmpegPuller puller;
    puller.Close();  // 应不崩溃
    puller.Close();
    PASS();
}

static void test_close_after_failed_open() {
    TEST("Close() after failed Open");
    FFmpegPuller puller;
    puller.Open("");        // 失败，内部调用了 Close
    puller.Close();          // 二次 close 应安全
    PASS();
}

// ── 测试 ReadPacket ───────────────────────────────────────────────

static void test_read_without_open() {
    TEST("ReadPacket() without Open returns false");
    FFmpegPuller puller;
    std::shared_ptr<MediaPacket> pkt;
    assert(!puller.ReadPacket(pkt));
    PASS();
}

static void test_read_after_close() {
    TEST("ReadPacket() after Close returns false");
    FFmpegPuller puller;
    assert(!puller.Open(""));
    puller.Close();
    std::shared_ptr<MediaPacket> pkt;
    assert(!puller.ReadPacket(pkt));
    PASS();
}

// ── 测试 GetStreamInfo ────────────────────────────────────────────

static void test_get_stream_info_before_open() {
    TEST("GetStreamInfo() ");
    FFmpegPuller puller;
    StreamInfo info = puller.GetStreamInfo();
    info.Dump();
    assert(info.stream_index == -1);
    assert(info.width == 0);
    PASS();
}

static void test_get_stream_info_after_open() { 
    TEST("GetStreamInfo() after Open returns non-empty info");
    FFmpegPuller puller;
    // puller.SetCredentials("admin", "3225");
    puller.Open("rtsp://192.168.10.7/live/mainstream");
    StreamInfo info = puller.GetStreamInfo();
    info.Dump();
    assert(info.stream_index != -1);
    assert(info.width != 0);
    PASS();
}

// ── main ───────────────────────────────────────────────────────────

int main() {
    LogManager::getInstance().Init();
    LOG_MAIN_INFO("=== FFmpegPuller tests ===");

    test_map_codec_id_h264();
    test_map_codec_id_hevc();
    test_map_codec_id_aac();
    test_map_codec_id_opus();
    test_map_codec_id_unknown();

    test_construct();
    test_open_empty_url();
    test_open_bogus_url();
    test_close_unopened();
    test_close_after_failed_open();
    test_read_without_open();
    test_read_after_close();
    test_get_stream_info_before_open();
    test_get_stream_info_after_open();

    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}
