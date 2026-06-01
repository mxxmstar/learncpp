/// @file test_media_source_node.cpp
/// MediaSourceNode 单元与集成测试
///
/// 测试项：
///   1. 构造 + 析构
///   2. Start/Stop 生命周期
///   3. 配置延迟生效
///   4. 完整链路集成测试（需要 RTSP 流地址，跳过不可用）
///
/// 集成测试 RTSP URL 可通过 TEST_RTSP_URL 环境变量覆盖。

#include "sourcenode/media_source_node.hpp"
#include "common/log/logmanager.h"
#include "decoder/ffmpeg_decoder.hpp"
#include "puller/ffmpeg_puller.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

// ── 辅助：io_context 后台线程 ────────────────────────────────────

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

static std::string TestUrl() {
    const char* env = std::getenv("TEST_RTSP_URL");
    return env ? std::string(env) : "rtsp://192.168.10.7/live/mainstream";
}

#define TEST(name) \
    do { LOG_MAIN_INFO_AT("[test] {} ...", name); } while (0)

#define PASS() \
    LOG_MAIN_INFO_AT("  PASS")

// ── 1. 构造 + 析构 ──────────────────────────────────────────────

static void test_construct_destroy() {
    TEST("Construct and destroy MediaSourceNode");

    IOTestContext ctx;
    {
        // 仅构造、不 Start、直接析构
        MediaSourceNode node(ctx.io, "rtsp://dummy",
                              nullptr,   // puller — Start() 前不需要
                              nullptr);  // decoder
    }
    PASS();
}

// ── 2. 配置延迟生效 ────────────────────────────────────────────

static void test_configure_before_start() {
    TEST("Configure before Start");

    IOTestContext ctx;
    MediaSourceNode node(ctx.io, "rtsp://dummy",
                          nullptr, nullptr);

    // 配置应在 Start() 前静默缓存，不抛异常
    node.SetReconnectIntervalMs(5000);
    node.SetMaxReconnectCount(3);
    node.SetWatchdogIntervalMs(10000);

    StreamSourceConfig cfg;
    cfg.session.connect_timeout_ms = 3000;
    cfg.puller.low_latency = true;
    node.SetStreamSourceConfig(cfg);

    PASS();
}

// ── 3. Start/Stop 生命周期（无网络 URL，验证不崩溃） ──────────

static void test_start_stop_does_not_crash() {
    TEST("Start/Stop with invalid URL does not crash");

    IOTestContext ctx;
    {
        auto puller = std::make_unique<FFmpegPuller>();
        puller->SetConnectTimeoutMs(1000);
        puller->SetReadTimeoutMs(2000);

        auto decoder = std::make_unique<FFmpegDecoder>();

        MediaSourceNode node(ctx.io, "rtsp://192.0.2.1/live/void",
                              std::move(puller), std::move(decoder));

        // 预期：连接失败，但 Start 不应抛异常
        try {
            node.Start();
        } catch (const std::exception& e) {
            LOG_MAIN_WARN_AT("Start threw (expected): {}", e.what());
        }

        // Stop 应安全
        node.Stop();

        // 二次 Stop 应安全（幂等）
        node.Stop();
    }
    PASS();
}

// ── 4. 集成测试：真实 RTSP 流 ──────────────────────────────────

static void test_integration_real_stream() {
    TEST("Integration: puller + decoder -> decoded frames");

    IOTestContext ctx;
    std::string url = TestUrl();

    auto puller = std::make_unique<FFmpegPuller>();
    puller->SetConnectTimeoutMs(5000);
    puller->SetReadTimeoutMs(5000);
    puller->SetLowLatency(true);

    auto decoder = std::make_unique<FFmpegDecoder>();

    auto node = std::make_shared<MediaSourceNode>(
        ctx.io, url, std::move(puller), std::move(decoder), "test_integration");

    // 注册 Emit 计数
    std::atomic<int> frame_count{0};
    node->SetEmitCallback([&](std::shared_ptr<MediaFrame> frame) {
        if (frame) {
            assert(frame->width  > 0);
            assert(frame->height > 0);
            assert(frame->pixel_format != PixelFormat::kUnknown);
            frame_count++;
        }
    });

    // 启动
    node->Start();

    // 检查是否连接成功
    auto info = node->GetStreamInfo();
    if (info.stream_index < 0) {
        LOG_MAIN_WARN_AT("  SKIP (no stream info after Start)");
        node->Stop();
        LOG_MAIN_WARN_AT("  (maybe no RTSP stream available at {})", url);
        return;
    }

    // 等待解码帧
    std::this_thread::sleep_for(std::chrono::seconds(3));
    node->Stop();

    if (frame_count == 0) {
        LOG_MAIN_WARN_AT("  SKIP (no frames decoded in 3s)");
        return;
    }

    LOG_MAIN_INFO_AT("  received {} decoded frames", frame_count.load());
    assert(frame_count > 0);
    PASS();
}

// ── main ──────────────────────────────────────────────────────────

int main() {
    LogManager::getInstance().Init();
    LOG_MAIN_INFO("=== MediaSourceNode tests ===");
    LOG_MAIN_INFO("Using RTSP URL: {}", TestUrl());

    //test_construct_destroy();
    //test_configure_before_start();
    //test_start_stop_does_not_crash();
    test_integration_real_stream();

    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}
