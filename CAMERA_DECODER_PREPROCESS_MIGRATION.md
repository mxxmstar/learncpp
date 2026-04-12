# Camera、Decoder、Preprocess 模块迁移完成报告

## 概述

成功将 camera（相机）、decoder（解码器）和 preprocess（预处理）三个模块从原来的 `include/` 和 `src/` 目录迁移到 `modules/` 目录。

## 迁移的模块

### 1. Camera 模块 📷

**文件结构：**
```
modules/camera/
├── CMakeLists.txt
├── include/camera/
│   ├── camera.h (1.7 KB)
│   ├── camera_httpclient.h (1.3 KB)
│   ├── camera_manager.h (2.2 KB)
│   ├── camera_storage.h (0.9 KB)
│   └── camera_stream.h (1.6 KB)
├── src/
│   └── camera.cpp (1.9 KB)
└── lib/
```

**依赖：**
- Boost::json
- log_lib
- net_lib

**功能：**
- 相机设备管理
- HTTP 客户端通信
- 相机状态管理
- 视频流处理

---

### 2. Decoder 模块 🎬

**文件结构：**
```
modules/decoder/
├── CMakeLists.txt
├── include/decoder/
│   ├── ffmpeg_decoder.h (2.8 KB)
│   └── i_decoder.h (3.4 KB)
├── src/
│   └── ffmpeg_decoder.cpp (8.7 KB)
└── lib/
```

**依赖：**
- FFmpeg 库 (avcodec, avformat, avutil, swresample, swscale)
- log_lib

**功能：**
- 视频解码接口抽象 (IDecoder)
- FFmpeg 解码器实现
- H.264/H.265 解码支持
- 通用视频帧结构 (VideoFrame)

---

### 3. Preprocess 模块 🖼️

**文件结构：**
```
modules/preprocess/
├── CMakeLists.txt
├── include/preprocess/
│   └── format_converter/
│       ├── i_format_converter.h (0.4 KB)
│       ├── opencv_format_converter.h (1.5 KB)
│       └── yuv_to_bgr_converter.h (1.5 KB)
├── src/
│   └── format_converter/
│       ├── opencv_format_converter.cpp (3.3 KB)
│       └── yuv_to_bgr_converter.cpp (3.8 KB)
└── lib/
```

**依赖：**
- OpenCV 库
- decoder_lib (VideoFrame 结构)
- log_lib

**功能：**
- 图像格式转换接口 (IFormatConverter)
- OpenCV 格式转换器
- YUV 到 BGR 转换器
- 视频帧预处理

---

## CMake 配置详情

### Camera 模块
```cmake
target_link_libraries(camera_lib
    PUBLIC
        Boost::json
        log_lib
        net_lib
)
```

### Decoder 模块
```cmake
target_link_libraries(decoder_lib
    PUBLIC
        ${FFMPEG_LIBRARIES}
        log_lib
)

# Windows 特定宏定义
target_compile_definitions(decoder_lib PUBLIC 
    WIN32_LEAN_AND_MEAN 
    NOMINMAX 
    __STDC_CONSTANT_MACROS 
    __STDC_LIMIT_MACROS
)
```

### Preprocess 模块
```cmake
target_link_libraries(preprocess_lib
    PUBLIC
        ${OpenCV_LIBS}
        decoder_lib  # preprocess 依赖 decoder 的 VideoFrame
        log_lib
)
```

---

## 主项目集成

在根 `CMakeLists.txt` 中：

### 1. 添加子模块（按依赖顺序）
```cmake
add_subdirectory(modules/log)
add_subdirectory(modules/net)
add_subdirectory(modules/puller)
add_subdirectory(modules/camera)      # ← 新增
add_subdirectory(modules/decoder)     # ← 新增
add_subdirectory(modules/preprocess)  # ← 新增
add_subdirectory(modules/sqlite)
add_subdirectory(modules/zlmediakit)
add_subdirectory(modules/config)
add_subdirectory(modules/web)
```

### 2. 链接到主程序
```cmake
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        log_lib
        net_lib
        puller_lib
        camera_lib      # ← 新增
        decoder_lib     # ← 新增
        preprocess_lib  # ← 新增
        sqlite_lib
        zlmediakit_lib
        config_lib
        web_lib
        ...
)
```

---

## 模块依赖关系图

```
主程序 (MySelfContainedApp)
├── log_lib
├── net_lib
│   └── log_lib
├── puller_lib
│   ├── Boost::asio
│   ├── Boost::beast
│   └── log_lib
├── camera_lib          ← 新模块
│   ├── Boost::json
│   ├── log_lib
│   └── net_lib
├── decoder_lib         ← 新模块
│   ├── FFmpeg Libraries
│   └── log_lib
├── preprocess_lib      ← 新模块
│   ├── OpenCV Libraries
│   ├── decoder_lib     # 依赖 VideoFrame
│   └── log_lib
├── sqlite_lib
│   └── SQLite3
│   └── log_lib
├── zlmediakit_lib
│   ├── nlohmann_json
│   ├── Boost::filesystem
│   ├── Boost::process
│   ├── log_lib
│   ├── net_lib
│   └── config_lib
├── config_lib
│   ├── yaml-cpp
│   ├── log_lib
│   └── net_lib
└── web_lib (api + service)
    ├── nlohmann_json
    ├── yaml-cpp
    ├── log_lib
    ├── net_lib
    ├── zlmediakit_lib
    └── config_lib
```

---

## 验证步骤

### 1. 重新配置 CMake

在 Visual Studio 中：
```
Project → Delete Cache and Reconfigure
```

或者命令行：
```powershell
Remove-Item -Recurse -Force out\build\x64-debug
cd out\build\x64-debug
cmake ../../.. -G "Visual Studio 17 2022" -A x64
```

### 2. 编译项目

```
Build → Build All
```

应该看到以下库成功编译：
- ✅ camera_lib
- ✅ decoder_lib
- ✅ preprocess_lib

### 3. 运行验证脚本

```powershell
.\verify_modules.ps1
```

应该显示所有 10 个模块都验证通过。

---

## 技术要点

### 1. FFmpeg 宏定义

Decoder 模块需要定义 FFmpeg 所需的宏：
- `__STDC_CONSTANT_MACROS` - C 标准常量宏
- `__STDC_LIMIT_MACROS` - C 标准限制宏

这些宏确保 FFmpeg 头文件正确编译。

### 2. OpenCV 集成

Preprocess 模块使用 OpenCV 进行图像格式转换：
- YUV → BGR 转换
- 各种像素格式转换
- 基于 OpenCV Mat 的处理

### 3. 相机管理

Camera 模块提供完整的相机生命周期管理：
- 相机发现和注册
- 状态监控（Offline/Online/Streaming）
- HTTP 通信
- 视频流管理

---

## 后续工作

### 可选优化

1. **添加单元测试**
   - 为每个模块创建 test/ 目录
   - 编写测试用例
   - 启用 BUILD_*_TESTS 选项

2. **添加文档**
   - 在每个模块目录下添加 README.md
   - 说明模块功能和用法
   - 提供示例代码

3. **完善错误处理**
   - 统一的错误码系统
   - 详细的错误日志
   - 异常安全保证

### 继续迁移其他模块

`include/` 和 `src/` 中还有以下目录可以迁移：
- `alg/` - 算法相关（包括 gRPC）
- `ffmpeg_opt/` - FFmpeg 优化相关
- `postprocess/` - 后处理相关（OSD 等）
- `videopipeline/`, `video_pipeline/` - 视频管道相关

---

## 注意事项

1. **Include 路径保持不变**
   - 代码中的 `#include "camera/camera.h"` 不需要修改
   - CMake 会自动处理路径映射

2. **依赖顺序很重要**
   - camera 在 net 之后（依赖 net_lib）
   - decoder 和 preprocess 可以在任何位置（只依赖外部库）

3. **FFmpeg 和 OpenCV 已通过 vcpkg 安装**
   - 无需额外安装
   - CMake 会自动找到这些库

---

## 相关文件

- `modules/camera/CMakeLists.txt` - Camera 模块配置
- `modules/decoder/CMakeLists.txt` - Decoder 模块配置
- `modules/preprocess/CMakeLists.txt` - Preprocess 模块配置
- `CMakeLists.txt` - 主项目配置（已更新）
- `verify_modules.ps1` - 验证脚本（已更新）

---

## 状态

✅ **Camera 模块迁移完成**
✅ **Decoder 模块迁移完成**
✅ **Preprocess 模块迁移完成**

可以进行编译验证了！🎉
