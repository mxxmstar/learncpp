/// @file test_ffmpeg_decoder.cpp
/// FFmpegDecoder 单元测试
///
/// 覆盖：
///   - MapAVCodecID / MapAVPixelFormat 映射函数
///   - Open / Close 生命周期
///   - 非法输入保护

#include "decoder/ffmpeg_decoder.hpp"
#include "common/log/logmanager.h"

#include <cassert>
#include <cstring>

#define TEST(name) \
    do { LOG_MAIN_INFO_AT("[test] {} ...", name); } while (0)

#define PASS() \
    LOG_MAIN_INFO_AT("  PASS")

// ── 映射函数测试 ────────────────────────────────────────────────

static void test_map_codec_id() {
    TEST("MapAVCodecID");

    assert(FFmpegDecoder::MapAVCodecID(AV_CODEC_ID_H264) == CodecType::H264);
    assert(FFmpegDecoder::MapAVCodecID(AV_CODEC_ID_HEVC) == CodecType::H265);
    assert(FFmpegDecoder::MapAVCodecID(AV_CODEC_ID_AAC)  == CodecType::AAC);
    assert(FFmpegDecoder::MapAVCodecID(AV_CODEC_ID_OPUS) == CodecType::OPUS);
    assert(FFmpegDecoder::MapAVCodecID(AV_CODEC_ID_NONE) == CodecType::UNKNOWN);

    PASS();
}

static void test_map_pixel_format() {
    TEST("MapAVPixelFormat");

    assert(FFmpegDecoder::MapAVPixelFormat(AV_PIX_FMT_NV12)    == PixelFormat::kNV12);
    assert(FFmpegDecoder::MapAVPixelFormat(AV_PIX_FMT_NV21)    == PixelFormat::kNV21);
    assert(FFmpegDecoder::MapAVPixelFormat(AV_PIX_FMT_YUV420P) == PixelFormat::kI420);
    assert(FFmpegDecoder::MapAVPixelFormat(AV_PIX_FMT_BGR24)   == PixelFormat::kBGR24);
    assert(FFmpegDecoder::MapAVPixelFormat(AV_PIX_FMT_RGB24)   == PixelFormat::kRGB24);
    assert(FFmpegDecoder::MapAVPixelFormat(AV_PIX_FMT_GRAY8)   == PixelFormat::kGRAY8);
    assert(FFmpegDecoder::MapAVPixelFormat(AV_PIX_FMT_NONE)    == PixelFormat::kUnknown);

    PASS();
}

// ── Open / Close 测试 ────────────────────────────────────────────

static void test_open_invalid_codec() {
    TEST("Open with UNKNOWN codec returns false");

    FFmpegDecoder dec;
    StreamInfo info;
    info.codec_type = CodecType::UNKNOWN;
    assert(!dec.Open(info));

    PASS();
}

static void test_open_h264_success() {
    TEST("Open H264 decoder with valid StreamInfo");

    FFmpegDecoder dec;
    StreamInfo info;
    info.media_type   = MediaType::VIDEO;
    info.codec_type   = CodecType::H264;
    info.width         = 1920;
    info.height        = 1080;

    bool ok = dec.Open(info);
    assert(ok);

    dec.Close();
    PASS();
}

static void test_open_hevc_success() {
    TEST("Open H265 decoder with valid StreamInfo");

    FFmpegDecoder dec;
    StreamInfo info;
    info.media_type   = MediaType::VIDEO;
    info.codec_type   = CodecType::H265;
    info.width         = 1280;
    info.height        = 720;

    bool ok = dec.Open(info);
    assert(ok);

    dec.Close();
    PASS();
}

static void test_double_open() {
    TEST("Double Open (close then reopen)");

    FFmpegDecoder dec;
    StreamInfo info;
    info.media_type   = MediaType::VIDEO;
    info.codec_type   = CodecType::H264;
    info.width         = 640;
    info.height        = 480;

    assert(dec.Open(info));
    dec.Close();

    // 重新打开
    assert(dec.Open(info));
    dec.Close();

    PASS();
}

static void test_open_with_extradata() {
    TEST("Open with extradata (SPS/PPS)");

    FFmpegDecoder dec;
    StreamInfo info;
    info.media_type   = MediaType::VIDEO;
    info.codec_type   = CodecType::H264;
    info.width         = 1920;
    info.height        = 1080;

    // 模拟 SPS + PPS（非真实有效，仅验证 extradata 通路）
    uint8_t dummy_extradata[] = {0x01, 0x42, 0x00, 0x1e, 0xe8, 0x40};
    info.extra_data.assign(dummy_extradata,
                           dummy_extradata + sizeof(dummy_extradata));

    bool ok = dec.Open(info);
    assert(ok);
    dec.Close();

    PASS();
}

static void test_close_without_open() {
    TEST("Close without Open (no-op)");

    FFmpegDecoder dec;
    dec.Close();  // 不应崩溃
    PASS();
}

// ── Decode 保护测试 ──────────────────────────────────────────────

static void test_decode_null_packet() {
    TEST("Decode with null packet returns false");

    FFmpegDecoder dec;
    assert(!dec.Decode(nullptr));

    PASS();
}

static void test_decode_without_open() {
    TEST("Decode without Open returns false");

    FFmpegDecoder dec;
    auto pkt = std::make_shared<MediaPacket>();
    assert(!dec.Decode(pkt));

    PASS();
}

// ── SetFrameCallback 测试 ────────────────────────────────────────

static void test_set_frame_callback() {
    TEST("SetFrameCallback and verify it stores");

    FFmpegDecoder dec;
    bool called = false;

    dec.SetFrameCallback([&](std::shared_ptr<MediaFrame>) {
        called = true;
    });

    // 此时回调不会立即触发，需要 Open + Decode
    // 这里仅验证 setter 不崩溃且可正常覆盖
    dec.SetFrameCallback(nullptr);  // 重置，不应崩溃

    PASS();
}

// ── main ───────────────────────────────────────────────────────────

int main() {
    LogManager::getInstance().Init();
    LOG_MAIN_INFO("=== FFmpegDecoder unit tests ===");

    test_map_codec_id();
    test_map_pixel_format();
    test_open_invalid_codec();
    test_open_h264_success();
    test_open_hevc_success();
    test_double_open();
    test_open_with_extradata();
    test_close_without_open();
    test_decode_null_packet();
    test_decode_without_open();
    test_set_frame_callback();

    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}
