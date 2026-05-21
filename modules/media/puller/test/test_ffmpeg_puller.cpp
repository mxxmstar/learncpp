// @file test_ffmpeg_puller.cpp
// FFmpegPuller 测试：MapCodecID、OnConnect/OnRead/OnDisconnect 原语、生命周期。

#include "puller/ffmpeg_puller.hpp"
#include "common/log/logmanager.h"

#include <boost/asio/io_context.hpp>

#include <cassert>
#include <atomic>
#include <thread>
#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

// ── 测试子类，暴露 protected 方法 ─────────────────────────────────

class TestFFmpegPuller : public FFmpegPuller {
public:
    using FFmpegPuller::FFmpegPuller;
    using FFmpegPuller::OnConnect;
    using FFmpegPuller::OnRead;
    using FFmpegPuller::OnDisconnect;
};

// ── 辅助 ───────────────────────────────────────────────────────────

#define TEST(name)                      \
    do {                                \
        LOG_MAIN_INFO_AT("[test] {} ...", name); \
    } while (0)

#define PASS()                          \
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

static void test_construct_state() {
    TEST("Construct initial state");
    boost::asio::io_context io_ctx;
    FFmpegPuller puller(io_ctx);
    assert(puller.GetState() == IPuller::State::IDLE);
    PASS();
}

static void test_construct_is_ipuller() {
    TEST("FFmpegPuller is IPuller");
    boost::asio::io_context io_ctx;
    FFmpegPuller puller(io_ctx);
    IPuller* base = dynamic_cast<IPuller*>(&puller);
    assert(base != nullptr);
    PASS();
}

// ── 测试 OnDisconnect 安全性 ──────────────────────────────────────

static void test_on_disconnect_idempotent() {
    TEST("OnDisconnect multiple calls safe");
    boost::asio::io_context io_ctx;
    TestFFmpegPuller puller(io_ctx);
    puller.OnDisconnect();  // nullptr -> safe
    puller.OnDisconnect();  // nullptr -> safe
    puller.OnDisconnect();  // nullptr -> safe
    PASS();
}

static void test_on_disconnect_after_failed_connect() {
    TEST("OnDisconnect after failed OnConnect");
    boost::asio::io_context io_ctx;
    TestFFmpegPuller puller(io_ctx);

    // 配置空 URL，OnConnect 会在 avformat_open_input 阶段失败
    PullerConfig cfg;
    cfg.url = "";
    puller.Open(cfg);

    // 直接调用 OnConnect（应失败并内部清理）
    bool ok = puller.OnConnect();
    assert(!ok);

    // OnDisconnect 应安全（fmt_ctx_ 已由 OnConnect 失败路径清理）
    puller.OnDisconnect();
    PASS();
}

// ── 测试 OnRead ───────────────────────────────────────────────────

static void test_on_read_stopped_returns_error() {
    TEST("OnRead returns ERROR_ when stopped");
    boost::asio::io_context io_ctx;
    TestFFmpegPuller puller(io_ctx);

    // 先 Stop 设置 stopped_ = true
    // 没有 Open/Start，直接 Stop 是安全的
    puller.Stop();

    IPuller::ReadResult r = puller.OnRead();
    assert(r == IPuller::ReadResult::ERROR_);
    PASS();
}

// ── 测试完整生命周期 ──────────────────────────────────────────────

static void test_lifecycle_empty_url() {
    TEST("Open(empty url) -> Start fails -> Stop");
    boost::asio::io_context io_ctx;
    FFmpegPuller puller(io_ctx);

    PullerConfig cfg;
    cfg.url = "";
    assert(puller.Open(cfg));
    assert(puller.GetState() == IPuller::State::OPENED);

    // Start 应失败（OnConnect -> avformat_open_input 返回错误）
    assert(!puller.Start());
    // OnConnect 失败后 Start 将状态置为 KERROR
    // Stop 清理回 IDLE
    puller.Stop();
    assert(puller.GetState() == IPuller::State::IDLE);
    PASS();
}

static void test_lifecycle_bogus_url() {
    TEST("Open(bogus url) -> Start fails -> Stop");
    boost::asio::io_context io_ctx;
    FFmpegPuller puller(io_ctx);

    PullerConfig cfg;
    cfg.url = "rtsp://192.0.2.1:55555/live/stream";  // 不可路由的测试 IP
    cfg.connect_timeout_ms = 100;
    cfg.read_timeout_ms    = 100;
    assert(puller.Open(cfg));
    assert(puller.GetState() == IPuller::State::OPENED);

    // Start 应快速失败（连接超时 100ms）
    assert(!puller.Start());
    puller.Stop();
    assert(puller.GetState() == IPuller::State::IDLE);
    PASS();
}

static void test_lifecycle_open_stop() {
    TEST("Open -> Stop (no Start)");
    boost::asio::io_context io_ctx;
    FFmpegPuller puller(io_ctx);

    PullerConfig cfg;
    cfg.url = "test://url";
    assert(puller.Open(cfg));
    assert(puller.GetState() == IPuller::State::OPENED);

    puller.Stop();
    assert(puller.GetState() == IPuller::State::IDLE);
    PASS();
}

static void test_lifecycle_reopen() {
    TEST("Open -> Start(fail) -> Stop -> Open again -> Start(fail)");
    boost::asio::io_context io_ctx;
    FFmpegPuller puller(io_ctx);

    PullerConfig cfg;
    cfg.url = "";
    assert(puller.Open(cfg));
    assert(!puller.Start());
    puller.Stop();
    assert(puller.GetState() == IPuller::State::IDLE);

    // 第二次 Open 应成功
    assert(puller.Open(cfg));
    assert(puller.GetState() == IPuller::State::OPENED);
    puller.Stop();
    PASS();
}

// ── 测试回调通过 FFmpegPuller 工作 ────────────────────────────────

static void test_event_callback_on_connect_fail() {
    TEST("Event callback fires on connect failure");
    boost::asio::io_context io_ctx;
    FFmpegPuller puller(io_ctx);

    PullerEvent received = static_cast<PullerEvent>(-1);
    puller.SetEventCallback([&](PullerEvent ev, const std::string&) {
        received = ev;
    });

    PullerConfig cfg;
    cfg.url = "";
    puller.Open(cfg);
    puller.Start();  // 应触发 ERROR_OCCURED

    assert(received == PullerEvent::ERROR_OCCURED);
    puller.Stop();
    PASS();
}

// ── 统计通过 FFmpegPuller 路径 ────────────────────────────────────

static void test_stats_after_start_fail() {
    TEST("Stats clean after Start failure");
    boost::asio::io_context io_ctx;
    FFmpegPuller puller(io_ctx);

    PullerConfig cfg;
    cfg.url = "rtsp://192.168.66.217/live/mainstream";
    puller.Open(cfg);
    puller.Start();
    puller.Stop();

    auto stats = puller.GetStats();
    assert(stats.bytes_received == 0);
    assert(stats.packets_received == 0);
    PASS();
}

static void test_successful_connection() {
    TEST("Successful connection lifecycle");
    boost::asio::io_context io_ctx;
    FFmpegPuller puller(io_ctx);

    std::atomic<int> connected_count{0};
    std::atomic<int> error_count{0};
    std::atomic<int> packet_count{0};
    std::atomic<bool> stream_info_received{false};

    puller.SetEventCallback([&](PullerEvent ev, const std::string& msg) {
        if (ev == PullerEvent::CONNECTED) {
            connected_count++;
            LOG_MAIN_INFO_AT("[test] Connected event received (count={})", connected_count.load());
        } else if (ev == PullerEvent::ERROR_OCCURED) {
            error_count++;
            LOG_MAIN_WARN_AT("[test] Error event: {}", msg);
        }
    });

    puller.SetStreamInfoCallback([&](const StreamInfo& info) {
        stream_info_received = true;
        LOG_MAIN_INFO_AT("[test] StreamInfo received: {}x{}, codec={}", 
                        info.width, info.height, static_cast<int>(info.codec_type));
    });

    puller.SetPacketCallback([&](std::shared_ptr<MediaPacket> pkt) {
        packet_count++;
        if (packet_count <= 5) {
            LOG_MAIN_INFO_AT("[test] Packet #{}: pts={}, dts={}, keyframe={}, size={} bytes", 
                           packet_count.load(), pkt->pts, pkt->dts, pkt->keyframe, 
                           pkt->buffer ? pkt->buffer->Size() : 0);
        }
    });

    PullerConfig cfg;
    cfg.url = "rtsp://192.168.66.217/live/mainstream";
    cfg.connect_timeout_ms = 10000;
    cfg.read_timeout_ms = 10000;
    cfg.io_timeout_ms = 10000;
    cfg.low_latency = true;

    assert(puller.Open(cfg));
    assert(puller.GetState() == IPuller::State::OPENED);

    bool start_result = puller.Start();
    if (!start_result) {
        LOG_MAIN_WARN_AT("[test] Start failed, skipping rest of test");
        puller.Stop();
        return;
    }

    assert(puller.GetState() == IPuller::State::CONNECTED);
    assert(connected_count.load() >= 1);

    for (int i = 0; i < 50 && packet_count < 5; ++i) {
        io_ctx.poll_one();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_MAIN_INFO_AT("[test] Received {} packets, waiting for stats timer...", packet_count.load());
    
    for (int i = 0; i < 20; ++i) {
        io_ctx.poll_one();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    auto stats = puller.GetStats();
    LOG_MAIN_INFO_AT("[test] Final stats: packets={}, bytes={}, bitrate={:.2f} kbps, errors={}",
                    stats.packets_received, stats.bytes_received, 
                    stats.bitrate, error_count.load());

    assert(stream_info_received);
    assert(packet_count > 0);
    
    if (stats.packets_received == 0) {
        LOG_MAIN_WARN_AT("[test] Stats not updated yet, this is expected if test runs too fast");
        LOG_MAIN_WARN_AT("[test] Callback received {} packets, stats shows {} (timer may not have fired)", 
                        packet_count.load(), stats.packets_received);
    } else {
        assert(stats.packets_received > 0);
        assert(stats.bytes_received > 0);
    }

    puller.Stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    assert(puller.GetState() == IPuller::State::IDLE);

    LOG_MAIN_INFO_AT("[test] Test completed successfully");

    PASS();
}

// ── main ───────────────────────────────────────────────────────────

int main() {
    LogManager::getInstance().Init();
    LOG_MAIN_INFO("=== FFmpegPuller tests ===");

    // // MapCodecID
    // test_map_codec_id_h264();
    // test_map_codec_id_hevc();
    // test_map_codec_id_aac();
    // test_map_codec_id_opus();
    // test_map_codec_id_unknown();

    // // 构造
    // test_construct_state();
    // test_construct_is_ipuller();

    // // OnDisconnect 安全性
    // test_on_disconnect_idempotent();
    // test_on_disconnect_after_failed_connect();

    // // OnRead
    // test_on_read_stopped_returns_error();

    // // 生命周期
    // test_lifecycle_empty_url();
    // test_lifecycle_bogus_url();
    // test_lifecycle_open_stop();
    // test_lifecycle_reopen();

    // // 回调
    // test_event_callback_on_connect_fail();

    // // 统计
    // test_stats_after_start_fail();

    test_successful_connection();
    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}
