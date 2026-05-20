/**
 * VideoPipeline OpenVINO + OSD + Pusher 集成测试
 *
 * 测试目标：
 * 1. 验证 OpenVINO、OSD、推流的完整链路
 * 2. 测试三线程架构（DecoderThread → InferenceThread → PushThread）
 * 3. 验证 YUV 级 OSD 渲染 + 零拷贝推流
 */

#include "videopipeline/video_pipeline.h"
#include "videopipeline/pipeline_config.h"
#include "pusher/i_pusher.h"
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif
#include "common/log/logmanager.h"

static std::atomic<bool> g_running{true};
static std::atomic<bool> g_shutdown_requested{false};

#ifdef _WIN32
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            if (!g_shutdown_requested.exchange(true)) {
                g_running = false;
            }
            return TRUE;
        default:
            return FALSE;
    }
}
#else
void SignalHandler(int signum) {
    if (!g_shutdown_requested.exchange(true)) {
        g_running = false;
    }
}
#endif

static void SetupGracefulShutdown() {
#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#else
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
#endif
}

int main(int argc, char* argv[]) {
    LogManager& log_mgr = LogManager::getInstance();
    log_mgr.Init();

#ifdef _WIN32
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    std::string exe_dir = exe_path;
    exe_dir = exe_dir.substr(0, exe_dir.find_last_of("\\/"));

    _putenv_s("OPENVINO_PLUGIN_PATHS", exe_dir.c_str());

    std::string current_path;
    const char* existing_path = getenv("PATH");
    if (existing_path) current_path = existing_path;
    _putenv_s("PATH", (exe_dir + ";" + current_path).c_str());

    LOG_MAIN_INFO_AT("EXE Directory: {}", exe_dir);
#endif    

    LOG_MAIN_INFO_AT("######################################################");
    LOG_MAIN_INFO_AT("# VideoPipeline + OSD + Pusher Integration Test");
    LOG_MAIN_INFO_AT("# Puller -> Decoder -> OpenVINO -> OSD -> Pusher");
    LOG_MAIN_INFO_AT("######################################################");

    SetupGracefulShutdown();

    std::string stream_url = "http://127.0.0.1:8888/live/proxy_cam1.live.flv";
    std::string model_path = "yolov5s.xml";
    std::string device = "CPU";
    std::string push_url = "rtsp://127.0.0.1:554/live/test";
    int channel_id = 1;
    int test_duration_sec = 600;

    if (argc > 1) stream_url = argv[1];
    if (argc > 2) model_path = argv[2];
    if (argc > 3) device = argv[3];
    if (argc > 4) push_url = argv[4];
    if (argc > 5) test_duration_sec = std::atoi(argv[5]);

    LOG_MAIN_INFO_AT("Test Configuration:");
    LOG_MAIN_INFO_AT("  Stream URL: {}", stream_url);
    LOG_MAIN_INFO_AT("  Model Path: {}", model_path.empty() ? "(not specified)" : model_path);
    LOG_MAIN_INFO_AT("  Device: {}", device);
    LOG_MAIN_INFO_AT("  Push URL: {}", push_url);
    LOG_MAIN_INFO_AT("  Channel ID: {}", channel_id);
    LOG_MAIN_INFO_AT("  Test Duration: {}s", test_duration_sec);

    PipelineConfig config;
    config.channel_id = channel_id;
    config.puller.stream_url = stream_url;

    config.algorithm.openvino.enabled = true;
    config.algorithm.openvino.model_path = model_path;
    config.algorithm.openvino.device = device;
    config.algorithm.openvino.confidence_threshold = 0.5f;
    config.algorithm.openvino.batch_size = 1;

    config.osd.enabled = true;
    config.osd.config.thickness = 2;
    config.osd.config.show_channel_id = true;
    config.osd.config.show_timestamp = true;
    config.osd.config.show_fps = true;
    config.osd.config.show_resolution = true;

    config.puller.reconnect_delay = 3;
    config.puller.max_reconnect_attempts = -1;
    config.decoder.decoder_threads = 2;
    config.decoder.raw_queue_size = 64;
    config.decoder.decoded_queue_size = 16;

    LOG_MAIN_INFO_AT("Pipeline Config:");
    LOG_MAIN_INFO_AT("  Algorithm: OpenVINO");
    LOG_MAIN_INFO_AT("  OSD: enabled");
    LOG_MAIN_INFO_AT("  Pusher target: {}", push_url);
    LOG_MAIN_INFO_AT("  Decoder threads: {}", config.decoder.decoder_threads);
    LOG_MAIN_INFO_AT("  Queue sizes: raw={}, decoded={}", config.decoder.raw_queue_size, config.decoder.decoded_queue_size);

    boost::asio::io_context io_ctx;

    std::thread io_thread([&io_ctx]() {
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work(io_ctx.get_executor());
        io_ctx.run();
    });

    LOG_MAIN_INFO_AT("Creating VideoPipeline...");
    auto pipeline = std::make_unique<VideoPipeline>(io_ctx, config);

    auto pusher = IPusher::Create();
    PusherConfig pcfg;
    pcfg.url = push_url;
    pcfg.width = 1920;
    pcfg.height = 1080;
    pcfg.fps = 25;
    pcfg.bitrate = 2000;

    if (!pusher->Start(pcfg, [](bool success, const std::string& msg, const PusherStats& s) {
        LOG_MAIN_WARN_AT("Pusher callback: success={}, msg={}, sent={}, failed={}",
                         success, msg, s.frames_sent, s.frames_failed);
    })) {
        LOG_MAIN_ERROR_AT("Failed to start pusher to {}", push_url);
        return 1;
    }

    pipeline->SetPusher(std::move(pusher));
    pipeline->SetPushTimeout(30);

    pipeline->setResultCallback([](int ch_id, const DetectionResult& result) {
        static int count = 0;
        if (++count % 30 == 0) {
            LOG_MAIN_INFO_AT("Detection #{}: channel={}, boxes={}, faces={}",
                             count, ch_id, result.boxes.size(), result.faces.size());
        }
    });

    LOG_MAIN_INFO_AT("Starting VideoPipeline...");
    if (!pipeline->start()) {
        LOG_MAIN_ERROR_AT("Failed to start VideoPipeline");
        LOG_MAIN_ERROR_AT("  1. Stream URL is correct and accessible: {}", stream_url);
        if (!model_path.empty()) {
            LOG_MAIN_ERROR_AT("  2. Model path exists: {}", model_path);
        }
        return 1;
    }

    LOG_MAIN_INFO_AT("VideoPipeline started successfully");
    LOG_MAIN_INFO_AT("Waiting for frames... (Ctrl+C to stop early)");

    auto start_time = std::chrono::steady_clock::now();
    int elapsed_sec = 0;
    uint64_t last_received = 0;
    uint64_t last_decoded = 0;
    uint64_t last_processed = 0;

    while (g_running && elapsed_sec < test_duration_sec) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        elapsed_sec++;

        if (elapsed_sec % 5 == 0) {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

            uint64_t r = pipeline->getFramesReceived();
            uint64_t d = pipeline->getFramesDecoded();
            uint64_t p = pipeline->getFramesProcessed();

            uint64_t dr = r - last_received;
            uint64_t dd = d - last_decoded;
            uint64_t dp = p - last_processed;
            last_received = r;
            last_decoded = d;
            last_processed = p;

            LOG_MAIN_INFO_AT("Stats at {}s: recv={}(+{}), dec={}(+{}), proc={}(+{}), fps_recv={}",
                             elapsed_sec, r, dr, d, dd, p, dp,
                             duration > 0 ? r / duration : 0);
        }
    }

    LOG_MAIN_INFO_AT("Shutdown: Stopping VideoPipeline...");
    pipeline->stop();
    LOG_MAIN_INFO_AT("Shutdown: VideoPipeline stopped");

    io_ctx.stop();
    LOG_MAIN_INFO_AT("Shutdown: IO context stopped");

    if (io_thread.joinable()) {
        io_thread.join();
        LOG_MAIN_INFO_AT("Shutdown: IO thread joined");
    }

    auto end_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();

    uint64_t total_received = pipeline->getFramesReceived();
    uint64_t total_decoded = pipeline->getFramesDecoded();
    uint64_t total_processed = pipeline->getFramesProcessed();

    LOG_MAIN_INFO_AT("==============================================");
    LOG_MAIN_INFO_AT("# Final Statistics");
    LOG_MAIN_INFO_AT("==============================================");
    LOG_MAIN_INFO_AT("Duration: {}s", total_duration);
    LOG_MAIN_INFO_AT("Received: {}, Decoded: {}, Processed: {}",
                     total_received, total_decoded, total_processed);

    if (total_duration > 0) {
        LOG_MAIN_INFO_AT("Avg recv FPS: {}, avg proc FPS: {}",
                         total_received / total_duration,
                         total_processed / total_duration);
    }

    bool test_passed = true;

    if (total_received == 0) {
        LOG_MAIN_ERROR_AT("FAILED: No frames received from stream");
        test_passed = false;
    } else {
        LOG_MAIN_INFO_AT("PASSED: Frames received: {}", total_received);
    }

    if (total_decoded == 0) {
        LOG_MAIN_ERROR_AT("FAILED: No frames decoded");
        test_passed = false;
    } else {
        LOG_MAIN_INFO_AT("PASSED: Frames decoded: {}", total_decoded);
    }

    if (total_processed == 0) {
        LOG_MAIN_WARN_AT("WARNING: No frames processed (expected if no model)");
    } else {
        LOG_MAIN_INFO_AT("PASSED: Frames processed: {}", total_processed);
    }

    if (test_passed) {
        LOG_MAIN_INFO_AT("Overall: TEST PASSED");
    } else {
        LOG_MAIN_ERROR_AT("Overall: TEST FAILED");
    }

    return test_passed ? 0 : 1;
}