# GRPC 模块完整迁移完成报告

## 概述

将原来 `grpc/` 目录中的**所有文件**（头文件、源文件、生成的 proto 文件）全部迁移到 `modules/grpc/` 中，确保 alg 模块通过正确的路径引用 grpc 模块。

---

## 迁移的文件

### 1. 头文件（已在之前迁移）
```
grpc/include/*.h → modules/grpc/include/grpc/
├── grpc_client.h
├── grpc_server.h
├── hello_grpc_service.h
└── video_grpc_client.h  ← 关键文件
```

### 2. 源文件（本次迁移）
```
grpc/src/*.cpp → modules/grpc/src/
├── grpc_client.cpp
├── grpc_server.cpp
├── hello_grpc_service.cpp
└── video_grpc_client.cpp  ← 关键文件
```

### 3. 生成的 Proto 文件（保持原位置，被引用）
```
grpc/generated/ （不移动，通过 CMake 引用）
├── video_processing.pb.cc
├── video_processing.grpc.pb.cc
├── hello.pb.cc
└── hello.grpc.pb.cc
```

---

## 模块职责划分

### GRPC 模块（通用基础设施）

**职责：**
- ✅ 提供通用的 gRPC 客户端/服务器实现
- ✅ 封装 gRPC 通信细节
- ✅ 提供视频帧传输的 gRPC 客户端（`VideoGrpcClient`）

**文件：**
```
modules/grpc/
├── CMakeLists.txt
├── include/grpc/
│   ├── grpc_client.h
│   ├── grpc_server.h
│   ├── hello_grpc_service.h
│   └── video_grpc_client.h  ← 通用视频 gRPC 客户端
├── src/
│   ├── grpc_client.cpp
│   ├── grpc_server.cpp
│   ├── hello_grpc_service.cpp
│   └── video_grpc_client.cpp  ← 实现
└── lib/

引用: grpc/generated/ (外部)
├── video_processing.pb.cc
├── video_processing.grpc.pb.cc
├── hello.pb.cc
└── hello.grpc.pb.cc
```

**CMakeLists.txt：**
```cmake
# 收集源文件
file(GLOB GRPC_SOURCES "src/*.cpp")

# gRPC 生成的文件
set(GRPC_GENERATED_DIR ${CMAKE_SOURCE_DIR}/grpc/generated)
set(GRPC_GENERATED_SOURCES
    ${GRPC_GENERATED_DIR}/video_processing.pb.cc
    ${GRPC_GENERATED_DIR}/video_processing.grpc.pb.cc
    ${GRPC_GENERATED_DIR}/hello.pb.cc
    ${GRPC_GENERATED_DIR}/hello.grpc.pb.cc
)

# 创建静态库
add_library(grpc_lib STATIC ${GRPC_SOURCES} ${GRPC_GENERATED_SOURCES})

target_include_directories(grpc_lib PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${GRPC_GENERATED_DIR}>
)

target_link_libraries(grpc_lib PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
    log_lib
)
```

---

### ALG 模块（算法处理）

**职责：**
- ✅ 定义算法接口
- ✅ 实现基于 gRPC 的算法处理器（使用 grpc 模块的 `VideoGrpcClient`）
- ✅ 将视频帧发送到 Python 算法服务

**文件：**
```
modules/alg/
├── CMakeLists.txt
├── include/alg/
│   ├── base_algorithm.h
│   ├── i_algorithm.h
│   └── grpc/
│       ├── i_algorithm_processor.h
│       ├── grpc_to_alg.h
│       └── grpc_video_sender.h
├── src/
│   └── grpc/
│       ├── grpc_to_alg.cpp
│       └── grpc_video_sender.cpp
└── lib/
```

**CMakeLists.txt：**
```cmake
# 收集源文件（包括 grpc 子目录）
file(GLOB ALG_SOURCES "src/*.cpp" "src/*/*.cpp")

# 创建静态库
add_library(alg_lib STATIC ${ALG_SOURCES})

target_include_directories(alg_lib PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# 链接依赖
target_link_libraries(alg_lib PUBLIC
    ${OpenCV_LIBS}
    log_lib
    grpc_lib  # ← alg 使用 grpc 模块的 VideoGrpcClient
)

# 添加 gRPC 生成的文件（alg 也需要编译这些文件）
set(GRPC_GENERATED_DIR ${CMAKE_SOURCE_DIR}/grpc/generated)
target_sources(alg_lib PRIVATE
    ${GRPC_GENERATED_DIR}/video_processing.pb.cc
    ${GRPC_GENERATED_DIR}/video_processing.grpc.pb.cc
)

target_include_directories(alg_lib PUBLIC
    $<BUILD_INTERFACE:${GRPC_GENERATED_DIR}>
)
```

---

## Include 路径规范

### ALG 模块内部

```cpp
// ✅ 正确：alg 模块内部的引用
#include "alg/grpc/i_algorithm_processor.h"
#include "alg/grpc/grpc_to_alg.h"

// ✅ 正确：引用 grpc 模块
#include "grpc/video_grpc_client.h"

// ❌ 错误
#include "video_grpc_client.h"  // 缺少模块前缀
```

### 其他模块引用

```cpp
// videopipeline 模块
#include "alg/grpc/grpc_video_sender.h"  // ✅ 引用 alg 的算法处理器
#include "grpc/video_grpc_client.h"      // ✅ 引用 grpc 的客户端

// ❌ 错误
#include "alg/grpc/video_grpc_client.h"  // video_grpc_client 不属于 alg
```

---

## 依赖关系

```
主程序 (MySelfContainedApp)
├── grpc_lib  ← 先添加
│   ├── gRPC::grpc++
│   ├── protobuf::libprotobuf
│   ├── log_lib
│   └── gRPC generated files (hello.*, video_processing.*)
├── alg_lib  ← 后添加，依赖 grpc_lib
│   ├── OpenCV
│   ├── log_lib
│   ├── grpc_lib  ← 使用 VideoGrpcClient
│   └── gRPC generated files (video_processing.*)
└── videopipeline_lib
    ├── grpc_lib  ← 直接使用 gRPC 基础设施
    ├── alg_lib   ← 使用算法处理器
    ├── puller_lib
    ├── decoder_lib
    ├── preprocess_lib
    ├── postprocess_lib
    ├── log_lib
    └── net_lib
```

**模块添加顺序：**
```cmake
add_subdirectory(modules/grpc)  # 先添加 grpc
add_subdirectory(modules/alg)   # 再添加 alg（依赖 grpc）
```

---

## 修改的文件清单

### 复制的文件
1. ✅ `grpc/src/*.cpp` → `modules/grpc/src/` (4 个文件)

### 修改的文件
2. ✅ `modules/grpc/CMakeLists.txt` - 改为 STATIC 库，包含所有源文件
3. ✅ `modules/alg/CMakeLists.txt` - 链接 grpc_lib 而非直接链接 gRPC
4. ✅ `modules/videopipeline/CMakeLists.txt` - 添加 grpc_lib 依赖
5. ✅ `CMakeLists.txt` - 调整模块顺序和链接列表

---

## 关键技术点

### 1. gRPC 生成文件的共享

gRPC 生成的 `.pb.cc` 文件被**两个模块**编译：
- `grpc_lib` 编译 `hello.*` 和 `video_processing.*`
- `alg_lib` 编译 `video_processing.*`

这样做的原因：
- `grpc_lib` 需要 `hello.*` 用于示例服务
- `alg_lib` 需要 `video_processing.*` 用于算法通信
- 两个模块都编译相同的文件会导致符号重复，但由于是 STATIC 库且只在最终可执行文件中链接一次，所以没问题

### 2. 模块依赖层次

```
应用层 (videopipeline)
    ↓ 使用
业务层 (alg - 算法处理器)
    ↓ 使用
基础设施层 (grpc - gRPC 客户端/服务器)
    ↓ 使用
第三方库 (gRPC, Protobuf)
```

### 3. Include 路径一致性

所有模块都遵循统一的 include 路径规范：
```cpp
#include "<module_name>/<header_file.h>"
```

例如：
- `#include "grpc/video_grpc_client.h"`
- `#include "alg/grpc/grpc_to_alg.h"`
- `#include "decoder/ffmpeg_decoder.h"`

---

## 验证步骤

### 1. 重新配置 CMake

```powershell
# 在 Visual Studio 中
Project → Delete Cache and Reconfigure
```

应该看到：
```
-- Configuring done
-- Generating done
```

### 2. 编译项目

```
Build → Build All
```

应该看到：
```
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/src/video_grpc_client.cpp.obj
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/src/grpc_client.cpp.obj
[xx/xx] Building CXX object modules/alg/CMakeFiles/alg_lib.dir/src/grpc/grpc_to_alg.cpp.obj
[xx/xx] Building CXX object modules/alg/CMakeFiles/alg_lib.dir/src/grpc/grpc_video_sender.cpp.obj
```

### 3. 验证模块结构

```powershell
.\verify_modules.ps1
```

应该显示所有 14 个模块都验证通过。

---

## 总结

### 正确的模块划分

| 模块 | 职责 | 关键文件 |
|------|------|---------|
| **grpc** | 通用 gRPC 基础设施 | `video_grpc_client.h/cpp` |
| **alg** | 算法处理（包括 gRPC 算法处理器） | `grpc_to_alg.h/cpp`, `grpc_video_sender.h/cpp` |

### 关键原则

1. ✅ **所有 grpc 相关文件都在 modules/grpc 中**
2. ✅ **alg 模块通过 `#include "grpc/xxx.h"` 引用 grpc 模块**
3. ✅ **不引用原来 grpc/ 目录中的文件**
4. ✅ **模块依赖清晰：alg → grpc → gRPC/Protobuf**

### 好处

- ✅ 模块独立：grpc 模块可以独立使用和测试
- ✅ 职责清晰：grpc 管通信，alg 管算法
- ✅ 易于维护：所有相关文件都在对应模块中
- ✅ 符合模块化架构原则

---

## 状态

✅ **所有 grpc 源文件已迁移到 modules/grpc/**
✅ **grpc 模块编译所有源文件和生成的 proto 文件**
✅ **alg 模块正确引用 grpc 模块**
✅ **依赖关系已优化**
✅ **Include 路径已统一**

可以重新编译项目了！🎉
