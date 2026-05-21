// @file test_ipuller.cpp
// IPuller 基类测试：使用 MockPuller 验证状态机、回调、重连、统计、超时。

#include "puller/i_puller.hpp"
#include "common/log/logmanager.h"

#include <boost/asio/io_context.hpp>

#include <cassert>
#include <vector>
#include <thread>
#include <chrono>

// ── MockPuller ─────────────────────────────────────────────────────

class MockPuller : public IPuller {
public:
    explicit MockPuller(boost::asio::io_context& io_ctx)
        : IPuller(io_ctx) {}

    // 模拟控制
    std::atomic<bool>   connect_ok{true};
    std::atomic<int>    connect_calls{0};
    std::atomic<int>    disconnect_calls{0};
    std::atomic<int>    read_calls{0};
    ReadResult          read_result{ReadResult::OK};
    uint64_t            bytes_per_read{100};
    int                 max_read_calls{-1};  // -1 = 无限制，>=0 表示最大调用次数
    bool                fail_after_first_connect{false};  // 首次连接后失败

    // 公开 protected 方法用于测试
    void TestDispatchPacket(std::shared_ptr<MediaPacket> mp) {
        DispatchPacket(mp);
    }
    
    void TestDispatchStreamInfo(const StreamInfo& info) {
        DispatchStreamInfo(info);
    }
    
    void TestDispatchEvent(PullerEvent ev, const std::string& msg = {}) {
        DispatchEvent(ev, msg);
    }
    
    uint64_t TestGetAsyncBytes() const {
        return async_bytes_received_.load();
    }
    
    uint64_t TestGetAsyncPackets() const {
        return async_packets_received_.load();
    }

protected:
    bool OnConnect() override {
        connect_calls++;
        
        // 如果设置了首次连接后失败，且已经调用过一次，返回 false
        if (fail_after_first_connect && connect_calls.load() > 1) {
            return false;
        }
        
        return connect_ok.load();
    }

    ReadResult OnRead() override {
        read_calls++;
        async_bytes_received_   += bytes_per_read;
        async_packets_received_++;
        
        // 如果设置了最大调用次数，超过后返回 EOF
        if (max_read_calls >= 0 && read_calls.load() > max_read_calls) {
            return ReadResult::EOF_;
        }
        
        return read_result;
    }

    void OnDisconnect() override {
        disconnect_calls++;
    }
};

// ── 辅助 ───────────────────────────────────────────────────────────

static std::vector<PullerEvent> g_events;
static void event_collector(PullerEvent ev, const std::string&) {
    g_events.push_back(ev);
}

#define TEST(name)                      \
    do {                                \
        LOG_MAIN_INFO("[test] {} ...", name); \
    } while (0)

#define PASS()                          \
    LOG_MAIN_INFO("  PASS")

// ── 测试用例 ──────────────────────────────────────────────────────

static void test_state_machine_idle() {
    TEST("state machine initial state");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);
    assert(puller.GetState() == IPuller::State::IDLE);
    PASS();
}

static void test_open_start_stop() {
    TEST("Open -> Start -> Stop");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);

    PullerConfig cfg;
    assert(puller.Open(cfg));
    assert(puller.GetState() == IPuller::State::OPENED);

    assert(puller.Start());
    assert(puller.GetState() == IPuller::State::CONNECTED);
    assert(puller.connect_calls == 1);

    puller.Stop();
    assert(puller.GetState() == IPuller::State::IDLE);
    assert(puller.disconnect_calls == 1);
    PASS();
}

static void test_open_twice_fails() {
    TEST("Open twice returns false");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);
    PullerConfig cfg;
    assert(puller.Open(cfg));
    assert(!puller.Open(cfg));   // 第二次 Open 应失败（状态已是 OPENED）
    PASS();
}

static void test_open_stop() {
    TEST("Open -> Stop (without Start)");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);
    PullerConfig cfg;
    assert(puller.Open(cfg));
    assert(puller.GetState() == IPuller::State::OPENED);
    puller.Stop();
    assert(puller.GetState() == IPuller::State::IDLE);
    PASS();
}

static void test_start_without_open_fails() {
    TEST("Start without Open returns false");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);
    assert(puller.Start() == false);
    PASS();
}

static void test_connect_failure() {
    TEST("OnConnect failure -> ERROR state");
    boost::asio::io_context io_ctx;
    
    {
        MockPuller puller(io_ctx);
        puller.connect_ok = false;

        PullerConfig cfg;
        assert(puller.Open(cfg));
        assert(!puller.Start());
        assert(puller.GetState() == IPuller::State::KERROR);
        
        // 手动 Stop，确保在 io_ctx 销毁前清理资源
        puller.Stop();
    }
    
    PASS();
}

// 注意：Start 失败后内部走 Stop 逻辑，状态回到 IDLE

static void test_callback_dispatch() {
    TEST("callback dispatch: stream info, packet, event");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);

    bool stream_info_called = false;
    bool packet_called      = false;
    bool event_called       = false;

    puller.SetStreamInfoCallback([&](const StreamInfo&) {
        stream_info_called = true;
    });
    puller.SetPacketCallback([&](std::shared_ptr<MediaPacket>) {
        packet_called = true;
    });
    puller.SetEventCallback([&](PullerEvent ev, const std::string&) {
        if (ev == PullerEvent::CONNECTED)
            event_called = true;
    });

    PullerConfig cfg;
    puller.Open(cfg);
    puller.Start();

    // 模拟 OnRead 中分发一个包
    auto mp = std::make_shared<MediaPacket>();
    puller.TestDispatchPacket(mp);

    StreamInfo info;
    puller.TestDispatchStreamInfo(info);
    puller.TestDispatchEvent(PullerEvent::CONNECTED);

    assert(stream_info_called);
    assert(packet_called);
    assert(event_called);

    puller.Stop();
    PASS();
}

static void test_workloop_calls_onread() {
    TEST("WorkLoop calls OnRead");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);

    PullerConfig cfg;
    cfg.read_timeout_ms = 10000;   // 足够长，避免超时
    puller.Open(cfg);
    puller.Start();

    // 限制 OnRead 最多调用 1 次，之后返回 EOF 停止循环
    puller.max_read_calls = 1;

    // poll() 执行 WorkLoop -> OnRead 被调用 1 次 -> 返回 EOF -> 停止循环
    io_ctx.poll();
    
    int n = puller.read_calls.load();

    // OnRead 应恰好被调用 2 次
    assert(n == 2);

    puller.Stop();
    PASS();
}

static void test_stats_accumulation() {
    TEST("async stats counters accumulate on OnRead");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);

    PullerConfig cfg;
    cfg.read_timeout_ms = 10000;
    puller.Open(cfg);
    puller.Start();

    // 限制 OnRead 最多调用 2 次，之后返回 EOF 停止循环
    puller.max_read_calls = 2;

    // WorkLoop 会多次调用 OnRead，每次累加 100 字节 + 1 包
    // 运行一段时间让 WorkLoop 执行几轮
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 100) {
        io_ctx.poll();
        std::this_thread::yield();
    }

    assert(puller.TestGetAsyncBytes() > 0);
    assert(puller.TestGetAsyncPackets() > 0);

    puller.Stop();
    PASS();
}

static void test_stats_timer_updates_stats() {
    TEST("stats timer updates bitrate/bytes/packets");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);

    PullerConfig cfg;
    cfg.read_timeout_ms = 100000;
    puller.Open(cfg);
    puller.Start();

    // 运行 1.1 秒，等 stats timer 至少触发一次
    // 用单独的线程跑 io_context
    std::thread t([&] { io_ctx.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    puller.Stop();
    t.join();

    // 停止后 stats 应包含累积数据
    auto stats = puller.GetStats();
    LOG_MAIN_INFO("  stats: bytes={}, packets={}, bitrate={} kbps",
                  stats.bytes_received, stats.packets_received, stats.bitrate);
    assert(stats.bytes_received > 0);
    assert(stats.packets_received > 0);
    assert(stats.bitrate > 0.0);
    PASS();
}

static void test_eof_stops_workloop() {
    TEST("OnRead returns EOF_ -> EOF event, loop stops");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);
    puller.read_result = IPuller::ReadResult::EOF_;

    bool eof_event = false;
    puller.SetEventCallback([&](PullerEvent ev, const std::string&) {
        if (ev == PullerEvent::EOF_REACHED) eof_event = true;
    });

    PullerConfig cfg;
    cfg.read_timeout_ms = 10000;
    puller.Open(cfg);
    puller.Start();

    // WorkLoop 执行一次 OnRead -> EOF -> 退出
    io_ctx.poll();

    assert(eof_event);
    // WorkLoop 不再 repost，后续 poll 无更多 OnRead
    int before = puller.read_calls.load();
    io_ctx.poll();
    int after = puller.read_calls.load();
    assert(after == before);  // 不再有新的 OnRead

    puller.Stop();
    PASS();
}

static void test_read_error_triggers_reconnect() {
    TEST("OnRead returns ERROR_ -> DoReconnect (auto_reconnect=true)");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);
    puller.read_result = IPuller::ReadResult::ERROR_;

    g_events.clear();
    puller.SetEventCallback(event_collector);

    PullerConfig cfg;
    cfg.read_timeout_ms    = 10000;
    cfg.auto_reconnect     = true;
    cfg.reconnect_interval_ms = 5;   // 快速重连

    puller.Open(cfg);
    puller.Start();

    // WorkLoop 执行 OnRead -> ERROR_ -> DoReconnect
    // 重连成功（默认 connect_ok=true）-> 重新派发 WorkLoop
    // 但 read_result 仍是 ERROR_，会再次触发重连
    std::thread t([&] { io_ctx.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    puller.Stop();
    t.join();

    // 至少 1 次 OnConnect + 1 次 OnDisconnect
    assert(puller.connect_calls >= 1);
    assert(puller.disconnect_calls >= 1);

    // RECONNECTING 事件至少发生一次
    bool found = false;
    for (auto ev : g_events) {
        if (ev == PullerEvent::RECONNECTING) { found = true; break; }
    }
    assert(found);

    puller.Stop();
    PASS();
}

static void test_read_error_no_reconnect() {
    TEST("OnRead ERROR_ + auto_reconnect=false -> stops");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);
    puller.read_result = IPuller::ReadResult::ERROR_;

    bool error_event = false;
    puller.SetEventCallback([&](PullerEvent ev, const std::string&) {
        if (ev == PullerEvent::ERROR_OCCURED) error_event = true;
    });

    PullerConfig cfg;
    cfg.read_timeout_ms = 10000;
    cfg.auto_reconnect  = false;

    puller.Open(cfg);
    puller.Start();
    io_ctx.poll();

    assert(error_event);

    puller.Stop();
    PASS();
}

static void test_connect_reconnect_limits() {
    TEST("reconnect respects max_reconnect_count");
    boost::asio::io_context io_ctx;
    MockPuller puller(io_ctx);
    puller.read_result              = IPuller::ReadResult::ERROR_;
    puller.fail_after_first_connect = true;   // 首次连接成功，重连失败

    g_events.clear();
    puller.SetEventCallback(event_collector);

    PullerConfig cfg;
    cfg.read_timeout_ms       = 10000;
    cfg.auto_reconnect        = true;
    cfg.reconnect_interval_ms = 5;
    cfg.max_reconnect_count   = 3;

    puller.Open(cfg);
    puller.Start();
    std::thread t([&] { io_ctx.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    puller.Stop();
    t.join();

    // 初始 OnConnect 成功（Start 中），之后三次重连尝试都失败
    assert(puller.connect_calls == 4);    // 1 次初始 + 3 次重试

    puller.Stop();
    PASS();
}

// ── main ───────────────────────────────────────────────────────────

int main() {
    LogManager& log_mgr = LogManager::getInstance();
    log_mgr.Init();
    LOG_MAIN_INFO("=== IPuller tests ===");

    test_state_machine_idle();
    test_open_start_stop();
    test_open_twice_fails();
    test_open_stop();
    test_start_without_open_fails();
    test_connect_failure();
    test_callback_dispatch();
    test_workloop_calls_onread();
    test_stats_accumulation();
    test_stats_timer_updates_stats();
    test_eof_stops_workloop();
    test_read_error_triggers_reconnect();
    test_read_error_no_reconnect();
    test_connect_reconnect_limits();

    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}
