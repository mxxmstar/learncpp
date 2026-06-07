#include "encodenode/encode_node.hpp"

#include "common/log/logmanager.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

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

static MediaFrame make_i420_frame(int width, int height, int64_t pts) {
    const int y_size = width * height;
    const int uv_width = width / 2;
    const int uv_height = height / 2;
    const int uv_size = uv_width * uv_height;

    std::vector<uint8_t> data(static_cast<size_t>(y_size + uv_size * 2));
    std::fill(data.begin(), data.begin() + y_size, 0x40);
    std::fill(data.begin() + y_size, data.begin() + y_size + uv_size, 0x80);
    std::fill(data.begin() + y_size + uv_size, data.end(), 0x80);

    MediaFrame frame;
    frame.type = MediaType::VIDEO;
    frame.pixel_format = PixelFormat::kI420;
    frame.width = width;
    frame.height = height;
    frame.stride[0] = width;
    frame.stride[1] = uv_width;
    frame.stride[2] = uv_width;
    frame.plane_offset[0] = 0;
    frame.plane_offset[1] = y_size;
    frame.plane_offset[2] = y_size + uv_size;
    frame.plane_count = 3;
    frame.pts = pts;
    frame.duration = 1;
    frame.buffer = std::make_shared<VectorMediaBuffer>(std::move(data));
    return frame;
}

static void test_push_without_init() {
    TEST("PushFrame without Init");

    EncodeNode node;
    assert(!node.PushFrame(make_i420_frame(64, 64, 0)));

    PASS();
}

static void test_inherits_inode() {
    TEST("EncodeNode inherits INode<FramePtr>");

    static_assert(std::is_base_of_v<common::runtime::INode<FramePtr>, EncodeNode>);

    PASS();
}

static void test_encode_callback() {
    TEST("EncodeNode Process callback");

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

    EncodeNode node;

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<PacketPtr> packets;

    node.SetPacketCallback([&](PacketPtr packet) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            packets.push_back(std::move(packet));
        }
        cv.notify_one();
    });

    if (!node.Init(cfg)) {
        LOG_MAIN_WARN_AT("  SKIP: H264 encoder is not available in this FFmpeg build");
        return;
    }

    node.Process(std::make_shared<MediaFrame>(
        make_i420_frame(cfg.width, cfg.height, 0)));
    node.Close();

    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, std::chrono::seconds(1), [&]() {
        return !packets.empty();
    });

    assert(!packets.empty());
    for (const auto& packet : packets) {
        assert(packet);
        assert(packet->type == MediaType::VIDEO);
        assert(packet->codec == CodecType::H264);
        assert(packet->buffer);
        assert(packet->buffer->Size() > 0);
    }

    PASS();
}

int main() {
    LogManager::getInstance().Init();
    LOG_MAIN_INFO("=== EncodeNode tests ===");

    test_push_without_init();
    test_inherits_inode();
    test_encode_callback();

    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}
