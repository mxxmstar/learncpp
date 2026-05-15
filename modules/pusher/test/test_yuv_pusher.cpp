#include "pusher/i_pusher.h"
#include "common/log/logmanager.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>

static void FillYuv420p(uint8_t* y, uint8_t* u, uint8_t* v, int w, int h, int ys, int uvs, int frame_num) {
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            y[row * ys + col] = static_cast<uint8_t>((frame_num * 25 + row + col) & 0xFF);
        }
    }
    int h2 = h / 2, w2 = w / 2;
    for (int row = 0; row < h2; ++row) {
        std::memset(u + row * uvs, 128, w2);
        std::memset(v + row * uvs, 128, w2);
    }
}

int main(int argc, char* argv[]) {
    LOG_MAIN_INFO_AT("=== YuvPusher Test ===");

    std::string rtsp_url = "rtsp://127.0.0.1:8554/live/test";
    int width = 640;
    int height = 480;
    int fps = 25;
    int duration_sec = 10;

    if (argc >= 2) rtsp_url = argv[1];
    if (argc >= 3) width = std::atoi(argv[2]);
    if (argc >= 4) height = std::atoi(argv[3]);
    if (argc >= 5) fps = std::atoi(argv[4]);
    if (argc >= 6) duration_sec = std::atoi(argv[5]);

    PusherConfig config;
    config.url = rtsp_url;
    config.width = width;
    config.height = height;
    config.fps = fps;
    config.bitrate = 1000;

    LOG_MAIN_INFO_AT("Config: url={}, {}x{} @ {}fps", config.url, width, height, fps);

    auto pusher = IPusher::Create();

    bool started = pusher->Start(config, [](bool success, const std::string& msg, const PusherStats& stats) {
        LOG_MAIN_INFO_AT("Callback: success={}, msg={}, sent={}, failed={}",
            success, msg, stats.frames_sent, stats.frames_failed);
    });

    if (!started) {
        LOG_MAIN_ERROR_AT("Failed to start pusher");
        return 1;
    }

    LOG_MAIN_INFO_AT("Pusher started, sending test frames for {} seconds...", duration_sec);

    int y_stride = width;
    int uv_stride = width / 2;
    std::vector<uint8_t> y_plane(y_stride * height);
    std::vector<uint8_t> u_plane(uv_stride * (height / 2));
    std::vector<uint8_t> v_plane(uv_stride * (height / 2));

    int frame_count = 0;
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(duration_sec);

    while (std::chrono::steady_clock::now() < end_time && pusher->IsRunning()) {
        FillYuv420p(y_plane.data(), u_plane.data(), v_plane.data(),
                    width, height, y_stride, uv_stride, frame_count);

        int64_t pts = frame_count * (1000 / fps);

        pusher->PushYuvFrame(y_plane.data(), u_plane.data(), v_plane.data(),
                            width, height, y_stride, uv_stride, pts);
        frame_count++;

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fps));
    }

    LOG_MAIN_INFO_AT("Sent {} frames, stopping...", frame_count);
    pusher->Stop();

    const auto& stats = pusher->GetStats();
    LOG_MAIN_INFO_AT("=== Test Complete ===");
    LOG_MAIN_INFO_AT("Frames sent: {}, failed: {}, rate: {:.1f}%",
        stats.frames_sent, stats.frames_failed, stats.getSuccessRate());

    return 0;
}