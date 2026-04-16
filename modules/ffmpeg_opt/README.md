# FFmpeg Opt 模块

## 📋 概述

FFmpeg 推流工具模块，提供常用的 FFmpeg 推流功能封装，主要用于将视频流推到 ZLMediaKit 服务器。

## 🎯 功能特性

- ✅ **RTSP 转 RTMP** - 将 RTSP 流转换为 RTMP 推到 ZLMediaKit
- ✅ **USB 摄像头推流** - 支持 Windows (DirectShow) 和 Linux (V4L2)
- ✅ **音频支持** - 可选音频编码（AAC）
- ✅ **跨平台** - Windows/Linux 自动适配
- ✅ **CMake 集成** - 自动查找 FFmpeg 可执行文件

## 📁 目录结构

```
ffmpeg_opt/
├── include/
│   └── ffmpeg_opt/
│       └── ffmpeg_opt.h          # 头文件
├── src/
│   └── ffmpeg_opt.cpp             # 实现文件
├── test/
│   ├── CMakeLists.txt             # 测试 CMake 配置
│   └── test_ffmpeg_opt.cpp        # 测试程序
├── CMakeLists.txt                 # 模块 CMake 配置
└── README.md                      # 本文档
```

## 🔧 CMake 配置

### 1. 自动查找 FFmpeg

模块会自动在 `tools/` 目录下查找 FFmpeg 可执行文件：

**Windows:**
```
tools/win32/ffmpeg-2025-05-01-git-707c04fe06-full_build/ffmpeg.exe
```

**Linux:**
```
tools/linux/ffmpeg_8_0/ffmpeg
```

### 2. 编译时宏定义

CMake 会定义 `FFMPEG_PATH` 宏，在编译时嵌入 FFmpeg 路径：

```cmake
target_compile_definitions(ffmpeg_opt_lib PRIVATE
    FFMPEG_PATH="${FFMPEG_PATH_NORMALIZED}"
)
```

### 3. 在主项目中集成

在主 `CMakeLists.txt` 中添加：

```cmake
add_subdirectory(modules/ffmpeg_opt)

# 链接到目标
target_link_libraries(your_target PRIVATE
    ffmpeg_opt_lib
)
```

## 💻 API 使用

### 1. RTSP 转 RTMP

```cpp
#include "ffmpeg_opt/ffmpeg_opt.h"

// 将 RTSP 流推到 ZLMediaKit
bool success = FFmpegOpt::PushRTSPToRTMP(
    "rtsp://192.168.66.166/live/mainstream",  // RTSP 源
    "rtmp://127.0.0.1:1935/live/proxy_cam1",  // RTMP 目标
    true  // copy_video: true=直接拷贝, false=重新编码
);
```

**生成的命令：**
```bash
ffmpeg.exe -rtsp_transport tcp -i rtsp://192.168.66.166/live/mainstream \
           -c:v copy -c:a aac -f flv rtmp://127.0.0.1:1935/live/proxy_cam1
```

### 2. USB 摄像头推流（无音频）

```cpp
#ifdef _WIN32
    std::string device = "USB2.0 UVC PC Camera";
#else
    std::string device = "/dev/video0";
#endif

bool success = FFmpegOpt::PushUSBCameraToRTMP(
    device,                        // 设备名称
    "rtmp://127.0.0.1:1935/live/cam1",  // RTMP 目标
    15,   // fps
    640,  // width
    480   // height
);
```

**生成的命令（Windows）：**
```bash
ffmpeg.exe -f dshow -rtbufsize 100M -i video="USB2.0 UVC PC Camera" \
           -r 15 -s 640x480 -c:v libx264 -preset ultrafast -tune zerolatency \
           -an -f flv rtmp://127.0.0.1:1935/live/cam1
```

### 3. USB 摄像头推流（有音频）

```cpp
#ifdef _WIN32
    std::string video_device = "USB2.0 UVC PC Camera";
    std::string audio_device = "USB2.0 UVC PC Camera";
#else
    std::string video_device = "/dev/video0";
    std::string audio_device = "hw:0,0";
#endif

bool success = FFmpegOpt::PushUSBCameraWithAudioToRTMP(
    video_device,
    audio_device,
    "rtmp://127.0.0.1:1935/live/cam1",
    15,   // fps
    640,  // width
    480   // height
);
```

**生成的命令（Windows）：**
```bash
ffmpeg.exe -f dshow -rtbufsize 100M \
           -i video="USB2.0 UVC PC Camera":audio="USB2.0 UVC PC Camera" \
           -r 15 -s 640x480 -c:v libx264 -preset ultrafast -tune zerolatency \
           -c:a aac -f flv rtmp://127.0.0.1:1935/live/cam1
```

### 4. 获取 FFmpeg 路径

```cpp
std::string path = FFmpegOpt::GetFFmpegPath();
if (!path.empty()) {
    std::cout << "FFmpeg: " << path << std::endl;
} else {
    std::cerr << "FFmpeg not found!" << std::endl;
}
```

### 5. 执行自定义命令

```cpp
std::string custom_cmd = R"(ffmpeg.exe -i input.mp4 -c:v libx264 output.mp4)";
bool success = FFmpegOpt::ExecuteCommand(custom_cmd);
```

## 🧪 运行测试

### 编译测试

```bash
# 启用测试
cmake -DBUILD_TESTS=ON ..
cmake --build .

# 运行测试
./bin/test_ffmpeg_opt
```

### 测试内容

测试程序包含以下测试用例（默认注释掉，需要手动启用）：

1. ✅ 获取 FFmpeg 路径
2. ⏸️ RTSP 转 RTMP（需要 RTSP 源）
3. ⏸️ USB 摄像头无音频（需要摄像头）
4. ⏸️ USB 摄像头有音频（需要摄像头+麦克风）

## 📝 注意事项

### 1. 阻塞调用

所有推流函数都是**阻塞调用**，会一直运行直到 FFmpeg 进程结束。

**建议在线程中运行：**

```cpp
#include <thread>

std::thread push_thread([]() {
    FFmpegOpt::PushRTSPToRTMP(rtsp_url, rtmp_url, true);
});

// 主线程继续执行其他任务...

// 停止推流（需要额外实现停止机制）
push_thread.join();
```

### 2. 设备名称

**Windows:**
- 查看可用设备：`ffmpeg -list_devices true -f dshow -i dummy`
- 常见名称：`"USB2.0 UVC PC Camera"`, `"Integrated Webcam"`

**Linux:**
- 查看可用设备：`v4l2-ctl --list-devices`
- 常见名称：`"/dev/video0"`, `"/dev/video1"`

### 3. 性能优化

- **低延迟模式**: 使用 `-preset ultrafast -tune zerolatency`
- **直接拷贝**: RTSP 转 RTMP 时使用 `copy_video=true` 避免重新编码
- **缓冲区大小**: USB 摄像头使用 `-rtbufsize 100M` 防止丢帧

### 4. ZLMediaKit 配置

确保 ZLMediaKit 已启动并监听 RTMP 端口（默认 1935）：

```bash
# 启动 ZLMediaKit
./MediaServer

# 验证 RTMP 流
ffplay rtmp://127.0.0.1:1935/live/proxy_cam1
```

## 🔍 故障排查

### 问题 1: FFmpeg 未找到

**症状:**
```
FFmpegOpt::GetFFmpegPath - FFmpeg not found
```

**解决:**
1. 检查 `tools/` 目录下是否有 FFmpeg
2. 手动设置 `FFMPEG_PATH` 环境变量
3. 修改 `CMakeLists.txt` 中的搜索路径

### 问题 2: 设备不存在

**症状:**
```
Could not find video device
```

**解决:**
1. 确认设备名称正确
2. Windows: 运行 `ffmpeg -list_devices true -f dshow -i dummy`
3. Linux: 运行 `v4l2-ctl --list-devices`

### 问题 3: 连接 ZLMediaKit 失败

**症状:**
```
Connection to rtmp://127.0.0.1:1935 failed
```

**解决:**
1. 确认 ZLMediaKit 已启动
2. 检查防火墙设置
3. 验证端口 1935 是否开放

## 📚 相关文档

- [ZLMediaKit 官方文档](https://github.com/ZLMediaKit/ZLMediaKit)
- [FFmpeg 官方文档](https://ffmpeg.org/documentation.html)
- [项目主 README](../../README.md)

## 📄 许可证

与主项目保持一致。
