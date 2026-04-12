# ALG、GRPC、Postprocess、VideoPipeline 模块迁移完成报告

## 概述

成功将 alg（算法）、grpc、postprocess（后处理）和 videopipeline（视频管道）四个模块从原来的 `include/` 和 `src/` 目录迁移到 `modules/` 目录。

## 迁移的模块

### 1. ALG 模块 🧠

**文件结构：**
```
modules/alg/
├── CMakeLists.txt
├── include/alg/
│   ├── base_algorithm.h (4.0 KB)
│   ├── i_algorithm.h (0.3 KB)
│   └── grpc/
│       ├── grpc_to_alg.h (1.9 KB)
│       ├── grpc_video_sender.h (1.8 KB)
│       └── i_algorithm_processor.h (3.4 KB)
├── src/
│   └── grpc/
│       ├── grpc_to_alg.cpp (5.7 KB)
│       └── grpc_video_sender.cpp (3.3 KB)
└── lib/
```

**依赖：**
- OpenCV 库
- log_lib

**功能：**
- 算法处理基础接口
- gRPC 算法处理器
- 视频数据发送到算法服务

---

### 2. GRPC 模块 🔌

**文件结构：**
```
modules/grpc/
├── CMakeLists.txt
├── include/grpc/
│   ├── grpc_to_alg.h
│   ├── grpc_video_sender.h
│   └── i_algorithm_processor.h
├── src/
│   ├── grpc_to_alg.cpp
│   └── grpc_video_sender.cpp
└── lib/
```

**依赖：**
- alg_lib (IAlgorithmProcessor 接口)
- gRPC::grpc++
- protobuf::libprotobuf
- log_lib

**功能：**
- gRPC 客户端实现
- 与远程算法服务通信
- 视频帧传输和结果接收

**注意：** grpc 模块实际上是 alg 模块的一部分，但为了模块化清晰，单独作为一个模块。

---

### 3. Postprocess 模块 🎨

**文件结构：**
```
modules/postprocess/
├── CMakeLists.txt
├── include/postprocess/
│   ├── result_output.h (2.2 KB)
│   └── osd/
│       └── osd_renderer.h (1.5 KB)
├── src/
│   └── osd/
│       └── osd_renderer.cpp (5.5 KB)
└── lib/
```

**依赖：**
- OpenCV 库
- decoder_lib (VideoFrame)
- log_lib

**功能：**
- 结果输出管理
- OSD（On-Screen Display）渲染
- 检测结果可视化

---

### 4. VideoPipeline 模块 🎬

**文件结构：**
```
modules/videopipeline/
├── CMakeLists.txt
├── include/videopipeline/
│   ├── frame_data.h (2.8 KB)
│   ├── frame_queue.h (4.4 KB)
│   ├── pipeline_config.h (3.3 KB)
│   ├── video_pipeline.h (4.5 KB)
│   └── video_pipeline_manager.h (3.8 KB)
├── src/
│   ├── video_pipeline.cpp (13.0 KB)
│   └── video_pipeline_manager.cpp (7.3 KB)
└── lib/
```

**依赖：**
- OpenCV 库
- decoder_lib
- preprocess_lib
- postprocess_lib
- alg_lib
- log_lib
- net_lib
- Boost::asio
- Boost::lockfree

**功能：**
- 完整的视频处理流水线
- 帧数据管理
- 无锁队列（高性能）
- 管道配置和管理
- 集成解码、预处理、算法、后处理

---

## CMake 配置详情

### ALG 模块
```cmake
target_link_libraries(alg_lib
    PUBLIC
        ${OpenCV_LIBS}
        log_lib
)
```

### GRPC 模块
```cmake
target_link_libraries(grpc_lib
    PUBLIC
        alg_lib  # grpc 依赖 alg 的 IAlgorithmProcessor
        gRPC::grpc++
        protobuf::libprotobuf
        log_lib
)
```

### Postprocess 模块
```cmake
target_link_libraries(postprocess_lib
    PUBLIC
        ${OpenCV_LIBS}
        decoder_lib  # 可能依赖 VideoFrame
        log_lib
)
```

### VideoPipeline 模块
```cmake
target_link_libraries(videopipeline_lib
    PUBLIC
        ${OpenCV_LIBS}
        decoder_lib
        preprocess_lib
        postprocess_lib
        alg_lib
        log_lib
        net_lib
        Boost::asio
        Boost::lockfree
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
add_subdirectory(modules/camera)
add_subdirectory(modules/decoder)
add_subdirectory(modules/preprocess)
add_subdirectory(modules/postprocess)      # ← 新增
add_subdirectory(modules/alg)              # ← 新增
add_subdirectory(modules/grpc)             # ← 新增
add_subdirectory(modules/videopipeline)    # ← 新增
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
        camera_lib
        decoder_lib
        preprocess_lib
        postprocess_lib      # ← 新增
        alg_lib              # ← 新增
        grpc_lib             # ← 新增
        videopipeline_lib    # ← 新增
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
├── camera_lib
│   ├── Boost::json
│   ├── log_lib
│   └── net_lib
├── decoder_lib
│   ├── FFmpeg Libraries
│   └── log_lib
├── preprocess_lib
│   ├── OpenCV Libraries
│   ├── decoder_lib
│   └── log_lib
├── postprocess_lib          ← 新模块
│   ├── OpenCV Libraries
│   ├── decoder_lib
│   └── log_lib
├── alg_lib                  ← 新模块
│   ├── OpenCV Libraries
│   └── log_lib
├── grpc_lib                 ← 新模块
│   ├── alg_lib
│   ├── gRPC::grpc++
│   ├── protobuf::libprotobuf
│   └── log_lib
├── videopipeline_lib        ← 新模块
│   ├── OpenCV Libraries
│   ├── decoder_lib
│   ├── preprocess_lib
│   ├── postprocess_lib
│   ├── alg_lib
│   ├── log_lib
│   ├── net_lib
│   ├── Boost::asio
│   └── Boost::lockfree
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

## 当前模块总数

现在项目共有 **14 个模块**：

1. log
2. net
3. puller
4. camera
5. decoder
6. preprocess
7. **postprocess** ← 新增
8. **alg** ← 新增
9. **grpc** ← 新增
10. **videopipeline** ← 新增
11. sqlite
12. zlmediakit
13. config
14. web (api + service 合并)

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
- ✅ alg_lib
- ✅ grpc_lib
- ✅ postprocess_lib
- ✅ videopipeline_lib

### 3. 运行验证脚本

```powershell
.\verify_modules.ps1
```

应该显示所有 14 个模块都验证通过。

---

## 技术要点

### 1. VideoPipeline 是核心模块

VideoPipeline 模块是整个视频处理系统的核心，它集成了：
- **解码** (decoder_lib)
- **预处理** (preprocess_lib)
- **算法处理** (alg_lib)
- **后处理** (postprocess_lib)
- **网络通信** (net_lib)
- **高性能队列** (Boost::lockfree)

### 2. GRPC 与 ALG 的关系

- `alg` 模块定义了算法处理的接口
- `grpc` 模块实现了基于 gRPC 的远程算法调用
- 两者可以独立使用，也可以配合使用

### 3. Postprocess 的功能

- **OSD 渲染**：在视频帧上绘制检测结果
- **结果输出**：格式化并输出算法结果
- 依赖 decoder 的 VideoFrame 结构

---

## 注意事项

1. **Include 路径保持不变**
   - 代码中的 `#include "alg/base_algorithm.h"` 不需要修改
   - CMake 会自动处理路径映射

2. **依赖顺序很重要**
   - alg 必须在 grpc 之前
   - postprocess 必须在 videopipeline 之前
   - 所有基础模块必须在 videopipeline 之前

3. **VideoPipeline 是重量级模块**
   - 依赖多个其他模块
   - 包含复杂的异步处理逻辑
   - 使用无锁队列提高性能

---

## 相关文件

- `modules/alg/CMakeLists.txt` - ALG 模块配置
- `modules/grpc/CMakeLists.txt` - GRPC 模块配置
- `modules/postprocess/CMakeLists.txt` - Postprocess 模块配置
- `modules/videopipeline/CMakeLists.txt` - VideoPipeline 模块配置
- `CMakeLists.txt` - 主项目配置（已更新）
- `verify_modules.ps1` - 验证脚本（已更新）

---

## 状态

✅ **ALG 模块迁移完成**
✅ **GRPC 模块迁移完成**
✅ **Postprocess 模块迁移完成**
✅ **VideoPipeline 模块迁移完成**

可以进行编译验证了！🎉
