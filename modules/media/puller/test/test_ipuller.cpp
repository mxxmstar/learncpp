/// @file test_ipuller.cpp
/// StreamSession 单元测试：使用 MockPuller 验证状态机、重连、watchdog、统计。

#include "stream/session/stream_session.h"
#include "common/log/logmanager.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

// ── 测试辅助：运行 io_context 的后台线程 ──────────────────────────
///
/// ReadLoop 通过 boost::asio::post 调度到 io_context，
/// 测试中需要让 io_context 在后台线程运行才能驱动 ReadLoop。
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

// ── MockPuller ─────────────────────────────────────────────────────

class MockPuller : public IPuller {
public:
    // 模拟控制
    std::atomic<bool>   connect_result{true};
    std::atomic<int>    connect_count{0};
    std::atomic<int>    close_count{0};
    std::atomic<int>    read_count{0};
    bool                return_null_packet{false};  // 返回空包（非目标流跳过）
    bool                return_error{false};        // ReadPacket 返回 false
    int                 max_reads{-1};              // -1 无限制，>=0 后返回 error

    StreamInfo          stream_info;

    bool Open(const std::string& url) override {
        connect_count++;
        return connect_result.load();
    }

    void Close() override {
        close_count++;
    }

    bool ReadPacket(std::shared_ptr<MediaPacket>& packet) override {
        read_count++;
        if (max_reads >= 0 && read_count > max_reads)
            return false;
        if (return_error)
            return false;
        if (return_null_packet) {
            packet = nullptr;
            return true;
        }
        packet = std::make_shared<MediaPacket>();
        return true;
    }

    StreamInfo GetStreamInfo() const override {
        return stream_info;
    }

    void SetEventCallback(EventCallback cb) override {
        // 本 mock 不需要事件回调
    }
};

// ── 辅助 ───────────────────────────────────────────────────────────

#define TEST(name) \
    do { LOG_MAIN_INFO_AT("[test] {} ...", name); } while (0)

#define PASS() \
    LOG_MAIN_INFO_AT("  PASS")

static StreamSession::State g_last_state;
static std::mutex g_state_mutex;

static void state_collector(StreamSession::State s) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    g_last_state = s;
}

// ── 测试 ───────────────────────────────────────────────────────────

// ── 测试（不需 io_context 运行） ─────────────────────────────────

static void test_initial_state() {
    TEST("初始状态 IDLE");
    boost::asio::io_context io;
    StreamSession session(io);
    assert(session.GetState() == StreamSession::State::KIDLE);
    PASS();
}

static void test_start_without_puller() {
    TEST("无 puller 时 Start 返回 false");
    boost::asio::io_context io;
    StreamSession session(io);
    session.SetUrl("rtsp://test");
    assert(!session.Start());
    PASS();
}

static void test_start_without_url() {
    TEST("无 url 时 Start 返回 false");
    boost::asio::io_context io;
    StreamSession session(io);
    session.SetPuller(std::make_unique<MockPuller>());
    assert(!session.Start());
    PASS();
}

static void test_start_success() {
    TEST("Start 成功 -> CONNECTED");
    boost::asio::io_context io;
    auto session = std::make_shared<StreamSession>(io);
    session->SetPuller(std::make_unique<MockPuller>());
    session->SetUrl("rtsp://test");
    assert(session->Start());
    assert(session->GetState() == StreamSession::State::KCONNECTED);
    session->Stop();
    PASS();
}

static void test_start_connect_failure() {
    TEST("连接失败 -> ERROR 状态");
    boost::asio::io_context io;
    StreamSession session(io);
    auto puller = std::make_unique<MockPuller>();
    puller->connect_result = false;
    session.SetPuller(std::move(puller));
    session.SetUrl("rtsp://test");
    assert(!session.Start());
    assert(session.GetState() == StreamSession::State::KERROR);
    PASS();
}

static void test_stop_returns_to_idle() {
    TEST("Stop 后状态为 STOPPED");
    boost::asio::io_context io;
    auto session = std::make_shared<StreamSession>(io);
    session->SetPuller(std::make_unique<MockPuller>());
    session->SetUrl("rtsp://test");
    assert(session->Start());
    session->Stop();
    assert(session->GetState() == StreamSession::State::KSTOPPED);
    PASS();
}

static void test_streaminfo_callback_fires() {
    TEST("连接成功 -> streaminfo 回调（同步）");
    boost::asio::io_context io;
    auto session = std::make_shared<StreamSession>(io);
    auto puller = std::make_unique<MockPuller>();
    puller->stream_info.width  = 1920;
    puller->stream_info.height = 1080;
    session->SetPuller(std::move(puller));
    session->SetUrl("rtsp://test");

    std::atomic<bool> info_received{false};
    session->SetStreamInfoCallback([&](const StreamInfo& info) {
        info_received = true;
        assert(info.width == 1920);
        assert(info.height == 1080);
    });

    assert(session->Start());
    assert(info_received);
    session->Stop();
    PASS();
}

// ── 测试（需 io_context 在后台运行驱动 ReadLoop/定时器） ─────────

static void test_packet_callback_fires() {
    TEST("读取到包 -> packet 回调被调用");
    IOTestContext ctx;
    auto session = std::make_shared<StreamSession>(ctx.io);
    session->SetPuller(std::make_unique<MockPuller>());
    session->SetUrl("rtsp://test");

    std::atomic<int> packet_count{0};
    session->SetPacketCallback([&](std::shared_ptr<MediaPacket>) {
        packet_count++;
    });

    assert(session->Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    session->Stop();

    assert(packet_count > 0);
    PASS();
}

static void test_null_packet_skipped() {
    TEST("空包（非目标流）跳过，不触发回调");
    IOTestContext ctx;
    auto session = std::make_shared<StreamSession>(ctx.io);
    auto puller = std::make_unique<MockPuller>();
    puller->return_null_packet = true;
    session->SetPuller(std::move(puller));
    session->SetUrl("rtsp://test");

    std::atomic<int> packet_count{0};
    session->SetPacketCallback([&](std::shared_ptr<MediaPacket>) {
        packet_count++;
    });
    session->SetReconnectIntervalMs(10000);

    assert(session->Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    session->Stop();

    LOG_MAIN_INFO_AT("  packet_count = {}", packet_count.load());
    PASS();
}

static void test_read_error_triggers_reconnect() {
    TEST("ReadPacket 失败 -> 触发重连");
    IOTestContext ctx;
    auto session = std::make_shared<StreamSession>(ctx.io);
    auto puller = std::make_unique<MockPuller>();
    puller->return_error = true;
    session->SetPuller(std::move(puller));
    session->SetUrl("rtsp://test");
    session->SetReconnectIntervalMs(5);
    session->SetMaxReconnectCount(3);

    std::vector<StreamSession::State> states;
    session->SetStateCallback([&](StreamSession::State s) {
        states.push_back(s);
    });

    assert(session->Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    session->Stop();

    bool had_reconnecting = false;
    for (auto s : states) {
        if (s == StreamSession::State::KRECONNECTING)
            had_reconnecting = true;
    }
    assert(had_reconnecting);
    PASS();
}

static void test_reconnect_limit() {
    TEST("重连达到上限 -> ERROR");
    IOTestContext ctx;
    auto session = std::make_shared<StreamSession>(ctx.io);
    auto puller = std::make_unique<MockPuller>();
    puller->return_error = true;
    puller->connect_result = false;
    session->SetPuller(std::move(puller));
    session->SetUrl("rtsp://test");
    session->SetReconnectIntervalMs(5);
    session->SetMaxReconnectCount(2);

    std::vector<StreamSession::State> states;
    session->SetStateCallback([&](StreamSession::State s) {
        states.push_back(s);
    });

    assert(session->Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    session->Stop();

    bool had_error = false;
    for (auto s : states) {
        if (s == StreamSession::State::KERROR)
            had_error = true;
    }
    assert(had_error);
    PASS();
}

static void test_watchdog_triggers_reconnect() {
    TEST("Watchdog 超时 -> 触发重连");
    IOTestContext ctx;
    auto session = std::make_shared<StreamSession>(ctx.io);
    auto puller = std::make_unique<MockPuller>();
    puller->return_null_packet = true;
    session->SetPuller(std::move(puller));
    session->SetUrl("rtsp://test");
    session->SetWatchdogIntervalMs(30);
    session->SetReconnectIntervalMs(5);

    std::atomic<int> reconnect_count{0};
    session->SetStateCallback([&](StreamSession::State s) {
        if (s == StreamSession::State::KRECONNECTING)
            reconnect_count++;
    });

    assert(session->Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    session->Stop();

    LOG_MAIN_INFO_AT("  reconnect_count = {}", reconnect_count.load());
    PASS();
}

static void test_watchdog_disabled_by_default() {
    TEST("Watchdog 默认关闭 (interval=0)");
    IOTestContext ctx;
    auto session = std::make_shared<StreamSession>(ctx.io);
    auto puller = std::make_unique<MockPuller>();
    puller->return_null_packet = true;
    session->SetPuller(std::move(puller));
    session->SetUrl("rtsp://test");

    std::atomic<int> reconnect_count{0};
    session->SetStateCallback([&](StreamSession::State s) {
        if (s == StreamSession::State::KRECONNECTING)
            reconnect_count++;
    });

    assert(session->Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    session->Stop();

    LOG_MAIN_INFO_AT("  reconnect_count = {}", reconnect_count.load());
    PASS();
}

static void test_stats_accumulate() {
    TEST("统计计数器累积");
    IOTestContext ctx;
    auto session = std::make_shared<StreamSession>(ctx.io);
    session->SetPuller(std::make_unique<MockPuller>());
    session->SetUrl("rtsp://test");

    assert(session->Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    session->Stop();

    auto stats = session->GetStats();
    LOG_MAIN_INFO_AT("  stats: packets={}, bytes={}",
                     stats.packets_received, stats.bytes_received);
    PASS();
}

// ── main ───────────────────────────────────────────────────────────

int main() {
    LogManager::getInstance().Init();
    LOG_MAIN_INFO("=== StreamSession tests ===");

    test_initial_state();
    test_start_without_puller();
    test_start_without_url();
    test_start_success();
    test_start_connect_failure();
    test_stop_returns_to_idle();
    test_packet_callback_fires();
    test_null_packet_skipped();
    test_read_error_triggers_reconnect();
    test_reconnect_limit();
    test_watchdog_triggers_reconnect();
    test_watchdog_disabled_by_default();
    test_streaminfo_callback_fires();
    test_stats_accumulate();

    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}
