# Pusher 模块

视频推流模块，支持通过 FFmpeg 将处理后的视频流推送到 ZLMediaKit。

## 架构

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   VideoPipeline │     │   FrameQueue    │     │   PusherService │
│   (BGR 输出)    │────▶│   (缓冲队列)    │────▶│   (独立服务)    │
└─────────────────┘     └─────────────────┘     └─────────────────┘
                                                      │
                                                      ▼
                                                ┌─────────────────┐
                                                │     FFmpeg      │
                                                │  (BGR→H264)     │
                                                └────────┬────────┘
                                                         │
                                                         ▼
                                                ┌─────────────────┐
                                                │   ZLMediaKit    │
                                                │   (RTSP)        │
                                                └─────────────────┘
```

## 文件结构

```
modules/pusher/
├── include/pusher/
│   ├── i_pusher.h          # 推流器基类接口
│   ├── pusher_config.h     # 推流配置
│   ├── bgr_to_yuv_converter.h  # BGR→YUV 转换器
│   └── yuv_pusher.h        # YUV 推流器实现
├── src/
│   ├── bgr_to_yuv_converter.cpp
│   └── yuv_pusher.cpp
└── CMakeLists.txt
```

## 使用方法

### 1. 创建推流器

```cpp
#include "pusher/i_pusher.h"

auto pusher = IPusher::Create();

PusherConfig config;
config.url = "rtsp://127.0.0.1:8554/live/stream";
config.width = 1920;
config.height = 1080;
config.fps = 25;
config.bitrate = 2000;
```

### 2. 启动推流

```cpp
pusher->Start(config, [](bool success, const std::string& msg, const PusherStats& stats) {
    LOG_INFO("Push status: success={}, sent={}, failed={}", success, stats.frames_sent, stats.frames_failed);
});
```

### 3. 推送帧

```cpp
// VideoPipeline 回调中推送帧
pipeline.setFrameOutputCallback([&](int ch_id, cv::Mat&& frame, int64_t pts) {
    pusher->PushFrame(frame, pts);
});
```

### 4. 停止推流

```cpp
pusher->Stop();
```

## 测试

编译后运行测试程序：

```bash
./bin/test_pusher_test_yuv_pusher.exe rtsp://127.0.0.1:8554/live/test 640 480 25 10
```

## FFmpeg 命令

推流使用以下 FFmpeg 命令：

```bash
ffmpeg -f rawvideo -pix_fmt bgr24 -s WxH -framerate FPS -i - \
       -c:v libx264 -preset ultrafast -tune zerolatency \
       -b:v BITRATEk -g GOP -pix_fmt yuv420p \
       -f rtsp -rtsp_transport tcp rtsp://target
```

## 依赖

- common_lib: 日志
- ffmpeg_opt_lib: FFmpeg 路径
- OpenCV: cv::Mat 处理