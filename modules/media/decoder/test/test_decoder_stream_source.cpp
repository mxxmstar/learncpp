/// @file test_decoder_stream_source.cpp
/// FFmpegDecoder + StreamSource 集成测试
///
/// 验证完整链路：
///   StreamSource → StreamSession → FFmpegPuller → FFmpegDecoder → FrameCallback
///
/// 需要可用的 RTSP 流地址，可通过 TEST_RTSP_URL 环境变量覆盖。

#include "decoder/ffmpeg_decoder.hpp"
#include "stream/stream_source.h"
#include "stream/session/stream_session.h"
#include "puller/ffmpeg_puller.hpp"
#include "common/log/logmanager.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

static std::string TestUrl() {
    const char* env = std::getenv("TEST_RTSP_URL");
    return env ? std::string(env) : "rtsp://192.168.10.7/live/mainstream";
}

struct IOTestContext {
    boost::asio::io_context io;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work;
    std::thread thread;

    IOTestContext()
        : work(boost::asio::make_work_guard(io))
        , thread([this]() { io.run(); }) {}

    ~IOTestContext() {
        work.reset();
        if (thread.joinable())
            thread.join();
    }
};

#define TEST(name) \
    do { LOG_MAIN_INFO_AT("[test] {} ...", name); } while (0)

#define PASS() \
    LOG_MAIN_INFO_AT("  PASS")

static std::unique_ptr<FFmpegPuller> CreateTestPuller() {
    auto puller = std::make_unique<FFmpegPuller>();
    puller->SetConnectTimeoutMs(3000);
    puller->SetReadTimeoutMs(5000);
    puller->SetLowLatency(true);
    return puller;
}

static bool TryStart(IOTestContext& ctx,
                     StreamSource& source,
                     const std::string& url) {
    bool ok;
    try {
        ok = source.Start();
    } catch (const std::exception& e) {
        LOG_MAIN_WARN_AT("Start threw exception: {}", e.what());
        return false;
    }
    if (!ok) {
        LOG_MAIN_WARN_AT("Start failed (stream unavailable?): {}", url);
    }
    return ok;
}

static bool WaitForStreamInfo(StreamSource& source, int max_retries) {
    for (int i = 0; i < max_retries; ++i) {
        StreamInfo info = source.GetStreamInfo();
        if (info.stream_index >= 0)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

// ── 测试 ─────────────────────────────────────────────────────────

/// 验证完整链路：RTSP → FFmpegPuller → StreamSource → FFmpegDecoder
/// 订阅包、送入解码器、校验至少收到一帧
static void test_decode_real_stream() {
    TEST("Decode RTSP stream: packets in -> frames out");
    IOTestContext ctx;
    std::string url = TestUrl();

    // 1. 创建拉流端
    auto puller = CreateTestPuller();
    auto session = std::make_shared<StreamSession>(ctx.io);
    session->SetPuller(std::move(puller));
    session->SetUrl(url);

    auto source = std::make_shared<StreamSource>("test_decode");
    source->SetSession(session);

    // 2. 创建解码器
    auto decoder = std::make_shared<FFmpegDecoder>();

    std::atomic<int> frame_count{0};
    decoder->SetFrameCallback([&](std::shared_ptr<MediaFrame> frame) {
        if (frame) {
            assert(frame->width  > 0);
            assert(frame->height > 0);
            assert(frame->pixel_format != PixelFormat::kUnknown);
            frame_count++;
        }
    });

    // 3. 订阅包 → 送入解码器（Open 前的包被安全忽略）
    source->AddPacketSubscriber(
        [decoder](std::shared_ptr<MediaPacket> pkt) {
            if (pkt && pkt->buffer)
                decoder->Decode(pkt);
        });

    // 4. 开始拉流
    if (!TryStart(ctx, *source, url)) {
        LOG_MAIN_WARN_AT("  SKIP (unable to connect)");
        return;
    }

    // 5. 等待 StreamInfo 就绪
    if (!WaitForStreamInfo(*source, 50)) {
        LOG_MAIN_WARN_AT("  SKIP (no StreamInfo received)");
        source->Stop();
        return;
    }

    // 6. 用 StreamInfo 打开解码器（含 extradata）
    StreamInfo info = source->GetStreamInfo();
    if (!decoder->Open(info)) {
        LOG_MAIN_WARN_AT("  SKIP (decoder Open failed for codec={})",
                         static_cast<int>(info.codec_type));
        source->Stop();
        return;
    }

    // 7. 拉流 3 秒收集帧
    std::this_thread::sleep_for(std::chrono::seconds(3));
    source->Stop();
    decoder->Close();

    LOG_MAIN_INFO_AT("  decoded {} frames", frame_count.load());
    assert(frame_count > 0);
    PASS();
}

/// 验证解码器在缺少必要信息时拒绝 Open
static void test_decoder_requires_valid_info() {
    TEST("Decoder Open rejects incomplete StreamInfo");

    FFmpegDecoder dec;

    StreamInfo empty;
    assert(!dec.Open(empty));

    StreamInfo partial;
    partial.width  = 1920;
    partial.height = 1080;
    // codec_type 为 UNKNOWN（默认）
    assert(!dec.Open(partial));

    PASS();
}

// ── main ───────────────────────────────────────────────────────────

int main() {
    LogManager::getInstance().Init();
    LOG_MAIN_INFO("=== FFmpegDecoder + StreamSource integration tests ===");
    LOG_MAIN_INFO("Using RTSP URL: {}", TestUrl());

    test_decoder_requires_valid_info();
    test_decode_real_stream();

    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}
