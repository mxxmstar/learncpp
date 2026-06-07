// FFmpegEncoder 单元测试
// 测试编码器配置校验、格式映射、帧编码及异常场景

#include "encoder/ffmpeg_encoder.hpp"

#include "common/log/logmanager.h"

#include <cassert>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

// 基于 vector 的 IMediaBuffer 实现，用于测试构造 MediaFrame
class VectorMediaBuffer : public IMediaBuffer {
public:
    explicit VectorMediaBuffer(std::vector<uint8_t> data)
        : data_(std::move(data)) {}

    uint8_t* Data() override { return data_.data(); }
    const uint8_t* Data() const override { return data_.data(); }
    size_t Size() const override { return data_.size(); }

private:
    std::vector<uint8_t> data_;
};

#define TEST(name) \
    do { LOG_MAIN_INFO_AT("[test] {} ...", name); } while (0)

#define PASS() \
    LOG_MAIN_INFO_AT("  PASS")

// 构造一个 I420 格式的测试帧
static FramePtr make_i420_frame(int width, int height, int64_t pts) {
    const int y_size = width * height;
    const int uv_width = width / 2;
    const int uv_height = height / 2;
    const int uv_size = uv_width * uv_height;

    std::vector<uint8_t> data(static_cast<size_t>(y_size + uv_size * 2));
    std::fill(data.begin(), data.begin() + y_size, 0x40);
    std::fill(data.begin() + y_size, data.begin() + y_size + uv_size, 0x80);
    std::fill(data.begin() + y_size + uv_size, data.end(), 0x80);

    auto frame = std::make_shared<MediaFrame>();
    frame->type = MediaType::VIDEO;
    frame->pixel_format = PixelFormat::kI420;
    frame->width = width;
    frame->height = height;
    frame->stride[0] = width;
    frame->stride[1] = uv_width;
    frame->stride[2] = uv_width;
    frame->plane_offset[0] = 0;
    frame->plane_offset[1] = y_size;
    frame->plane_offset[2] = y_size + uv_size;
    frame->plane_count = 3;
    frame->pts = pts;
    frame->duration = 1;
    frame->buffer = std::make_shared<VectorMediaBuffer>(std::move(data));
    return frame;
}

// 测试 CodecType <-> AVCodecID 映射
static void test_map_codec_type() {
    TEST("MapCodecType");

    assert(FFmpegEncoder::MapCodecType(CodecType::H264) == AV_CODEC_ID_H264);
    assert(FFmpegEncoder::MapCodecType(CodecType::H265) == AV_CODEC_ID_HEVC);
    assert(FFmpegEncoder::MapCodecType(CodecType::UNKNOWN) == AV_CODEC_ID_NONE);

    PASS();
}

// 测试 PixelFormat <-> AVPixelFormat 映射
static void test_map_pixel_format() {
    TEST("MapPixelFormat");

    assert(FFmpegEncoder::MapPixelFormat(PixelFormat::kI420) == AV_PIX_FMT_YUV420P);
    assert(FFmpegEncoder::MapPixelFormat(PixelFormat::kNV12) == AV_PIX_FMT_NV12);
    assert(FFmpegEncoder::MapPixelFormat(PixelFormat::kRGB24) == AV_PIX_FMT_RGB24);
    assert(FFmpegEncoder::MapPixelFormat(PixelFormat::kUnknown) == AV_PIX_FMT_NONE);

    PASS();
}

// 测试非法配置参数被正确拒绝
static void test_open_invalid_config() {
    TEST("Open invalid config");

    FFmpegEncoder enc;
    EncoderConfig cfg;
    cfg.width = 0;
    cfg.height = 64;
    assert(!enc.Open(cfg));

    PASS();
}

// 测试编码一帧 I420 数据并验证输出包格式正确
static void test_encode_one_i420_frame() {
    TEST("Encode one I420 frame");

    EncoderConfig cfg;
    cfg.codec_type = CodecType::H264;
    cfg.pixel_format = PixelFormat::kI420;
    cfg.width = 64;
    cfg.height = 64;
    cfg.fps_num = 25;
    cfg.fps_den = 1;
    cfg.bitrate = 400'000;
    cfg.gop_size = 10;
    cfg.max_b_frames = 0;

    if (!avcodec_find_encoder(FFmpegEncoder::MapCodecType(cfg.codec_type))) {
        LOG_MAIN_WARN_AT("  SKIP: H264 encoder is not available in this FFmpeg build");
        return;
    }

    FFmpegEncoder enc;
    assert(enc.Open(cfg));

    std::vector<PacketPtr> packets;
    assert(enc.Encode(make_i420_frame(cfg.width, cfg.height, 0), packets));
    assert(enc.Encode(nullptr, packets));  // flush

    assert(!packets.empty());
    for (const auto& packet : packets) {
        assert(packet);
        assert(packet->type == MediaType::VIDEO);
        assert(packet->codec == CodecType::H264);
        assert(packet->buffer);
        assert(packet->buffer->Size() > 0);
        assert(packet->backend.type == BackendHandle::FFMPEG);
        assert(packet->backend.ptr != nullptr);
    }

    enc.Close();
    PASS();
}

// 测试未 Open 时 Encode 应该返回 false
static void test_encode_without_open() {
    TEST("Encode without Open");

    FFmpegEncoder enc;
    std::vector<PacketPtr> packets;
    assert(!enc.Encode(make_i420_frame(64, 64, 0), packets));

    PASS();
}

int main() {
    LogManager::getInstance().Init();
    LOG_MAIN_INFO("=== FFmpegEncoder unit tests ===");

    test_map_codec_type();
    test_map_pixel_format();
    test_open_invalid_config();
    test_encode_without_open();
    test_encode_one_i420_frame();

    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}
