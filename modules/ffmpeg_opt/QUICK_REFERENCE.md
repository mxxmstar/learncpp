# FFmpeg Opt 快速参考

## 🚀 快速开始

### 1. CMake 集成

```cmake
# 在主 CMakeLists.txt 中
add_subdirectory(modules/ffmpeg_opt)

target_link_libraries(your_app PRIVATE ffmpeg_opt_lib)
```

### 2. 包含头文件

```cpp
#include "ffmpeg_opt/ffmpeg_opt.h"
```

---

## 📝 API 速查

### RTSP → RTMP

```cpp
FFmpegOpt::PushRTSPToRTMP(
    "rtsp://192.168.66.166/live/mainstream",  // RTSP 源
    "rtmp://127.0.0.1:1935/live/proxy_cam1",  // RTMP 目标
    true  // copy_video (true=低延迟, false=重新编码)
);
```

**生成命令**:
```bash
ffmpeg -rtsp_transport tcp -i rtsp://... -c:v copy -c:a aac -f flv rtmp://...
```

---

### USB Camera → RTMP (无音频)

```cpp
FFmpegOpt::PushUSBCameraToRTMP(
    "USB2.0 UVC PC Camera",  // Windows 设备名
    "rtmp://127.0.0.1:1935/live/cam1",
    15,   // FPS
    640,  // Width
    480   // Height
);
```

**生成命令**:
```bash
ffmpeg -f dshow -rtbufsize 100M -i video="..." -r 15 -s 640x480 \
       -c:v libx264 -preset ultrafast -tune zerolatency -an -f flv rtmp://...
```

---

### USB Camera + Audio → RTMP

```cpp
FFmpegOpt::PushUSBCameraWithAudioToRTMP(
    "USB2.0 UVC PC Camera",   // Video device
    "USB2.0 UVC PC Camera",   // Audio device
    "rtmp://127.0.0.1:1935/live/cam1",
    15, 640, 480
);
```

**生成命令**:
```bash
ffmpeg -f dshow -rtbufsize 100M -i video="...":audio="..." \
       -r 15 -s 640x480 -c:v libx264 -preset ultrafast -tune zerolatency \
       -c:a aac -f flv rtmp://...
```

---

## 🔍 常用设备名称

### Windows

**查看设备**:
```bash
ffmpeg -list_devices true -f dshow -i dummy
```

**常见设备**:
- 摄像头: `"USB2.0 UVC PC Camera"`, `"Integrated Webcam"`
- 麦克风: `"麦克风 (Realtek Audio)"`, `"USB2.0 UVC PC Camera"`

### Linux

**查看设备**:
```bash
v4l2-ctl --list-devices
arecord -l  # 音频设备
```

**常见设备**:
- 摄像头: `/dev/video0`, `/dev/video1`
- 麦克风: `hw:0,0`, `plughw:0,0`

---

## ⚙️ 参数调优

### 低延迟模式

```cpp
// 已在函数内部默认启用
-preset ultrafast -tune zerolatency
```

### 高质量模式

```cpp
// 需要修改源码或使用 ExecuteCommand()
-c:v libx264 -preset slow -crf 23
```

### 降低码率

```cpp
// 需要修改源码或使用 ExecuteCommand()
-b:v 1M -maxrate 1M -bufsize 2M
```

---

## ⚠️ 注意事项

### 1. 阻塞调用

所有函数都是**阻塞**的，建议在后台线程运行：

```cpp
std::thread t([]() {
    FFmpegOpt::PushRTSPToRTMP(...);
});
t.detach();  // 或 t.join()
```

### 2. ZLMediaKit 准备

确保 ZLMediaKit 已启动：

```bash
./MediaServer  # 启动 ZLMediaKit
ffplay rtmp://127.0.0.1:1935/live/proxy_cam1  # 验证
```

### 3. 防火墙设置

开放端口：
- RTMP: 1935
- HTTP-FLV: 8080 (如果需要)
- HLS: 8080 (如果需要)

---

## 🐛 故障排查

### FFmpeg 未找到

```
FFmpegOpt::GetFFmpegPath - FFmpeg not found
```

**解决**:
```bash
# 检查路径
ls tools/win32/ffmpeg*/ffmpeg.exe  # Windows
ls tools/linux/ffmpeg_8_0/ffmpeg   # Linux

# 或设置环境变量
export FFMPEG_PATH=/path/to/ffmpeg
```

### 设备不存在

```
Could not find video device
```

**解决**:
```bash
# Windows
ffmpeg -list_devices true -f dshow -i dummy

# Linux
v4l2-ctl --list-devices
```

### 连接失败

```
Connection to rtmp://127.0.0.1:1935 failed
```

**解决**:
```bash
# 检查 ZLMediaKit 是否运行
ps aux | grep MediaServer

# 检查端口
netstat -an | grep 1935

# 测试连接
telnet 127.0.0.1 1935
```

---

## 📊 性能对比

| 场景 | CPU 占用 | 延迟 | 备注 |
|------|---------|------|------|
| RTSP→RTMP (copy) | < 5% | ~1s | 直接拷贝，最低延迟 |
| RTSP→RTMP (encode) | 30-50% | ~2s | 重新编码 |
| USB Camera 640x480 | 20-30% | ~500ms | i5 CPU |
| USB Camera 1920x1080 | 60-80% | ~1s | i5 CPU |

---

## 💡 最佳实践

### 1. RTSP 流推荐配置

```cpp
// 低延迟：直接拷贝
FFmpegOpt::PushRTSPToRTMP(rtsp_url, rtmp_url, true);

// 兼容性：重新编码
FFmpegOpt::PushRTSPToRTMP(rtsp_url, rtmp_url, false);
```

### 2. USB 摄像头推荐配置

```cpp
// 低分辨率、低帧率（适合网络传输）
FFmpegOpt::PushUSBCameraToRTMP(device, rtmp_url, 15, 640, 480);

// 高分辨率、高帧率（适合本地预览）
FFmpegOpt::PushUSBCameraToRTMP(device, rtmp_url, 30, 1920, 1080);
```

### 3. 多路推流

```cpp
std::vector<std::thread> threads;

// 启动多路推流
for (int i = 0; i < 4; ++i) {
    threads.emplace_back([i]() {
        std::string url = "rtsp://camera_" + std::to_string(i) + "/stream";
        std::string rtmp = "rtmp://127.0.0.1:1935/live/cam" + std::to_string(i);
        FFmpegOpt::PushRTSPToRTMP(url, rtmp, true);
    });
}

// 等待所有推流结束
for (auto& t : threads) {
    t.join();
}
```

---

## 📚 更多信息

- 详细文档: [README.md](README.md)
- 重构总结: [REFACTOR_SUMMARY.md](REFACTOR_SUMMARY.md)
- 测试程序: `test/test_ffmpeg_opt.cpp`
