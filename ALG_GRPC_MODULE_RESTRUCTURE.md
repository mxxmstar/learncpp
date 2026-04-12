# ALG 和 GRPC 模块正确划分

## 核心概念

### 正确的模块职责

**ALG 模块（算法模块）**：
- ✅ 算法接口定义（`i_algorithm.h`, `base_algorithm.h`）
- ✅ **gRPC 算法处理器实现**（`grpc_to_alg.cpp`, `grpc_video_sender.cpp`）
- ✅ gRPC 算法处理器接口（`i_algorithm_processor.h`）
- ✅ 通过 gRPC 将视频帧发送到 Python 算法服务

**GRPC 模块（通用 gRPC 基础设施）**：
- ✅ 通用的 gRPC 客户端/服务器基础类
- ✅ Proto 生成的文件（`.pb.cc`, `.grpc.pb.cc`）
- ❌ **不包含**算法相关的 gRPC 实现

---

## 为什么这样划分？

### 业务逻辑角度

```
┌─────────────────────────────────────┐
│     VideoPipeline (视频处理流水线)    │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│      ALG Module (算法模块)           │
│  ┌───────────────────────────────┐  │
│  │ GrpcToAlg                     │  │
│  │ - 接收视频帧                   │  │
│  │ - 通过 gRPC 发送到 Python     │  │
│  │ - 接收检测结果                 │  │
│  └───────────────────────────────┘  │
└──────────────┬──────────────────────┘
               │ 使用 gRPC 协议
               ▼
┌─────────────────────────────────────┐
│   Python Algorithm Service          │
│   (YOLOv5, 目标检测等)               │
└─────────────────────────────────────┘
```

`GrpcToAlg` 是**算法处理的一种实现方式**，它属于算法模块的业务逻辑，而不是通用的 gRPC 基础设施。

### 代码复用角度

- **grpc 模块**：提供通用的 gRPC 工具，可以被任何模块使用
- **alg 模块**：专注于算法处理，可以选择不同的通信方式（gRPC、REST API、本地调用等）

---

## 文件结构

### ALG 模块

```
modules/alg/
├── CMakeLists.txt
├── include/alg/
│   ├── base_algorithm.h         # 算法基类
│   ├── i_algorithm.h            # 算法接口
│   └── grpc/                    # gRPC 算法处理器
│       ├── i_algorithm_processor.h
│       ├── grpc_to_alg.h
│       └── grpc_video_sender.h
├── src/
│   └── grpc/                    # gRPC 算法处理器实现
│       ├── grpc_to_alg.cpp
│       └── grpc_video_sender.cpp
└── lib/
```

**CMakeLists.txt 关键点：**
```cmake
# 收集源文件（包括 grpc 子目录）
file(GLOB ALG_SOURCES "src/*.cpp" "src/*/*.cpp")

# 创建静态库
add_library(alg_lib STATIC ${ALG_SOURCES})

# 链接依赖
target_link_libraries(alg_lib
    PUBLIC
        ${OpenCV_LIBS}
        log_lib
        gRPC::grpc++              # ← alg 需要 gRPC
        protobuf::libprotobuf     # ← alg 需要 protobuf
)

# 添加 gRPC 生成的文件
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

### GRPC 模块

```
modules/grpc/
├── CMakeLists.txt
├── include/grpc/
│   ├── grpc_client.h            # 通用 gRPC 客户端
│   ├── grpc_server.h            # 通用 gRPC 服务器
│   ├── hello_grpc_service.h     # 示例服务
│   └── video_grpc_client.h      # 视频 gRPC 客户端（通用）
├── src/                         # 目前可能为空
└── lib/

引用: grpc/generated/ (外部目录)
├── video_processing.pb.cc       # ← 编译进 alg_lib
├── video_processing.grpc.pb.cc  # ← 编译进 alg_lib
├── hello.pb.cc                  # ← 编译进 grpc_lib
├── hello.grpc.pb.cc             # ← 编译进 grpc_lib
└── *.h 头文件
```

**CMakeLists.txt 关键点：**
```cmake
# grpc 模块目前没有自己的源文件，只提供头文件和生成的 proto 文件
file(GLOB GRPC_SOURCES "src/*.cpp")

if(GRPC_SOURCES)
    add_library(grpc_lib STATIC ${GRPC_SOURCES} ${GRPC_GENERATED_SOURCES})
else()
    add_library(grpc_lib INTERFACE)  # ← INTERFACE 库
endif()

target_link_libraries(grpc_lib
    INTERFACE  # ← INTERFACE 关键字
        gRPC::grpc++
        protobuf::libprotobuf
        log_lib
)
```

---

## Include 路径规范

### ALG 模块内部

```cpp
// ✅ 正确
#include "alg/grpc/grpc_to_alg.h"
#include "alg/grpc/i_algorithm_processor.h"
#include "alg/base_algorithm.h"

// ❌ 错误
#include "grpc/grpc_to_alg.h"
```

### 其他模块引用 ALG

```cpp
// videopipeline 模块
#include "alg/grpc/grpc_video_sender.h"  // ✅ 正确
```

### GRPC 模块

```cpp
// ✅ 正确
#include "grpc/video_grpc_client.h"
#include "grpc/grpc_client.h"
```

---

## 依赖关系

```
主程序
└── videopipeline_lib
    ├── alg_lib  ← 包含 gRPC 算法处理器
    │   ├── OpenCV
    │   ├── log_lib
    │   ├── gRPC::grpc++
    │   ├── protobuf::libprotobuf
    │   └── gRPC generated files (video_processing.*)
    ├── puller_lib
    ├── decoder_lib
    ├── preprocess_lib
    ├── postprocess_lib
    ├── log_lib
    └── net_lib

grpc_lib (INTERFACE)  ← 只提供头文件和生成文件
├── gRPC::grpc++
├── protobuf::libprotobuf
└── log_lib
```

---

## 修改的文件清单

### 移动的文件
1. ✅ `modules/grpc/include/grpc/grpc_to_alg.h` → `modules/alg/include/alg/grpc/`
2. ✅ `modules/grpc/include/grpc/grpc_video_sender.h` → `modules/alg/include/alg/grpc/`
3. ✅ `modules/grpc/include/grpc/i_algorithm_processor.h` → `modules/alg/include/alg/grpc/`
4. ✅ `modules/grpc/src/grpc_to_alg.cpp` → `modules/alg/src/grpc/`
5. ✅ `modules/grpc/src/grpc_video_sender.cpp` → `modules/alg/src/grpc/`

### 修改的文件
6. ✅ `modules/alg/CMakeLists.txt` - 从 INTERFACE 改为 STATIC，添加 gRPC 依赖
7. ✅ `modules/grpc/CMakeLists.txt` - 简化为 INTERFACE 库
8. ✅ `modules/alg/src/grpc/grpc_to_alg.cpp` - 修正 include 路径
9. ✅ `modules/alg/src/grpc/grpc_video_sender.cpp` - 修正 include 路径
10. ✅ `modules/alg/include/alg/grpc/grpc_to_alg.h` - 修正 include 路径
11. ✅ `modules/videopipeline/include/videopipeline/video_pipeline.h` - 修正 include 路径
12. ✅ `modules/videopipeline/CMakeLists.txt` - 移除 grpc_lib 依赖
13. ✅ `CMakeLists.txt` - 移除 grpc_lib 链接

---

## 验证

重新配置并编译：

```powershell
# 在 Visual Studio 中
Project → Delete Cache and Reconfigure
Build → Build All
```

应该看到：
```
[xx/xx] Building CXX object modules/alg/CMakeFiles/alg_lib.dir/src/grpc/grpc_to_alg.cpp.obj
[xx/xx] Building CXX object modules/alg/CMakeFiles/alg_lib.dir/src/grpc/grpc_video_sender.cpp.obj
[xx/xx] Building CXX object modules/alg/CMakeFiles/alg_lib.dir/__/__/grpc/generated/video_processing.pb.cc.obj
```

---

## 总结

### 关键原则

1. **业务逻辑归属**：gRPC 只是通信手段，算法处理器属于算法模块
2. **模块独立性**：grpc 模块提供通用基础设施，alg 模块专注算法业务
3. **依赖清晰**：alg_lib 包含所有算法相关功能，使用者只需链接 alg_lib

### 好处

- ✅ 职责清晰：alg 管理算法，grpc 管理基础设施
- ✅ 易于扩展：可以轻松添加其他算法实现（本地、REST API 等）
- ✅ 依赖简单：videopipeline 只需链接 alg_lib，不需要关心通信细节

---

## 状态

✅ **alg 和 grpc 模块已正确划分**
✅ **文件已移动到正确位置**
✅ **Include 路径已修正**
✅ **CMakeLists.txt 已更新**
✅ **依赖关系已优化**

可以重新编译项目了！🎉
