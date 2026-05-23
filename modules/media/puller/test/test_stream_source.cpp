/// @file test_stream_source.cpp
/// StreamSource + FFmpegPuller integration test
///
/// Exercises the full pipeline:
///   StreamSource -> StreamSession -> FFmpegPuller -> av_read_frame
///
/// RTSP URL configurable via TEST_RTSP_URL env var.

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

// ── Test URL ─────────────────────────────────────────────────────────

static std::string TestUrl() {
    const char* env = std::getenv("TEST_RTSP_URL");
    return env ? std::string(env) : "rtsp://192.168.66.219/live/mainstream";
}

// ── Test harness: io_context background thread ────────────────────────

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

// ── Helper ────────────────────────────────────────────────────────────

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

// ── Tests ─────────────────────────────────────────────────────────────

static void test_construct() {
    TEST("Construct StreamSource");
    StreamSource source("test_stream");
    PASS();
}

static void test_start_without_session() {
    TEST("Start without session returns false");
    StreamSource source("test_stream");
    assert(!source.Start());
    PASS();
}

static void test_get_stream_info_before_start() {
    TEST("GetStreamInfo() before Start");
    StreamSource source("test_stream");
    StreamInfo info = source.GetStreamInfo();
    assert(info.stream_index == -1);
    assert(info.width == 0);
    PASS();
}

static void test_get_stream_info_after_start() {
    TEST("GetStreamInfo() after Start (FFmpeg real stream)");
    IOTestContext ctx;
    std::string url = TestUrl();

    auto puller = CreateTestPuller();

    auto session = std::make_shared<StreamSession>(ctx.io);
    session->SetPuller(std::move(puller));
    session->SetUrl(url);

    auto source = std::make_shared<StreamSource>("test_stream");
    source->SetSession(session);

    if (!TryStart(ctx, *source, url)) {
        LOG_MAIN_WARN_AT("  SKIP (unable to connect)");
        return;
    }

    StreamInfo info = source->GetStreamInfo();
    info.Dump();
    assert(info.stream_index >= 0);
    assert(info.width        > 0);
    assert(info.height       > 0);
    assert(info.codec_type   != CodecType::UNKNOWN);

    source->Stop();
    PASS();
}

static void test_packet_subscriber_fires() {
    TEST("Packet subscriber receives (FFmpeg real stream)");
    IOTestContext ctx;
    std::string url = TestUrl();

    auto puller = CreateTestPuller();

    auto session = std::make_shared<StreamSession>(ctx.io);
    session->SetPuller(std::move(puller));
    session->SetUrl(url);

    auto source = std::make_shared<StreamSource>("test_stream");
    source->SetSession(session);

    std::atomic<int> packet_count{0};
    source->AddPacketSubscriber([&](std::shared_ptr<MediaPacket>) {
        packet_count++;
    });

    if (!TryStart(ctx, *source, url)) {
        LOG_MAIN_WARN_AT("  SKIP (unable to connect)");
        return;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    source->Stop();

    assert(packet_count > 0);
    PASS();
}

static void test_multiple_subscribers() {
    TEST("Multiple subscribers receive (FFmpeg real stream)");
    IOTestContext ctx;
    std::string url = TestUrl();

    auto puller = CreateTestPuller();

    auto session = std::make_shared<StreamSession>(ctx.io);
    session->SetPuller(std::move(puller));
    session->SetUrl(url);

    auto source = std::make_shared<StreamSource>("test_stream");
    source->SetSession(session);

    std::atomic<int> count1{0}, count2{0};
    source->AddPacketSubscriber([&](std::shared_ptr<MediaPacket>) { count1++; });
    source->AddPacketSubscriber([&](std::shared_ptr<MediaPacket>) { count2++; });

    if (!TryStart(ctx, *source, url)) {
        LOG_MAIN_WARN_AT("  SKIP (unable to connect)");
        return;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    source->Stop();

    assert(count1 > 0);
    assert(count2 > 0);
    PASS();
}

static void test_stop_stops_session() {
    TEST("Stop changes session state to STOPPED");
    IOTestContext ctx;
    std::string url = TestUrl();

    auto puller = CreateTestPuller();

    auto session = std::make_shared<StreamSession>(ctx.io);
    session->SetPuller(std::move(puller));
    session->SetUrl(url);

    auto source = std::make_shared<StreamSource>("test_stream");
    source->SetSession(session);

    if (!TryStart(ctx, *source, url)) {
        LOG_MAIN_WARN_AT("  SKIP (unable to connect)");
        return;
    }

    source->Stop();
    assert(session->GetState() == StreamSession::State::KSTOPPED);
    PASS();
}

// ── main ───────────────────────────────────────────────────────────

int main() {
    LogManager::getInstance().Init();
    LOG_MAIN_INFO("=== StreamSource + FFmpegPuller tests ===");
    LOG_MAIN_INFO("Using RTSP URL: {}", TestUrl());

    test_construct();
    test_start_without_session();
    test_get_stream_info_before_start();
    test_get_stream_info_after_start();
    test_packet_subscriber_fires();
    test_multiple_subscribers();
    test_stop_stops_session();

    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}
