# GRPC 和 ALG 模块依赖关系修正

## 问题

之前的模块划分错误，导致 **grpc 模块依赖 alg 模块**，这是不正确的。

正确的依赖关系应该是：
- **grpc 模块** → 提供通用的 gRPC 通信基础设施
- **alg 模块** → 依赖 grpc 模块，实现具体的算法处理器

---

## 正确的模块职责

### GRPC 模块（通用基础设施）

**职责：** 提供与业务无关的 gRPC 通信基础能力

**包含内容：**
1. ✅ `grpc_client.h/cpp` - 通用 gRPC 客户端基类
2. ✅ `grpc_server.h/cpp` - 通用 gRPC 服务器基类
3. ✅ `hello_grpc_service.h/cpp` - Hello 服务示例（测试用）
4. ✅ `hello.proto` 生成的文件

**特点：**
- ❌ 不包含 OpenCV 依赖
- ❌ 不包含业务逻辑
- ✅ 可以被任何模块复用

---

### ALG 模块（算法相关）

**职责：** 实现算法处理相关的功能，包括通过 gRPC 与 Python 算法服务通信

**包含内容：**
1. ✅ `i_algorithm_processor.h` - 算法处理器接口
2. ✅ `grpc_to_alg.h/cpp` - 通过 gRPC 发送视频到算法服务
3. ✅ `grpc_video_sender.h/cpp` - 视频发送器
4. ✅ `video_grpc_client.h/cpp` - 视频处理 gRPC 客户端 ← **从 grpc 移到这里**
5. ✅ `video_processing.proto` 生成的文件

**特点：**
- ✅ 包含 OpenCV 依赖（处理视频帧）
- ✅ 包含算法业务逻辑
- ✅ 依赖 grpc 模块的通用基础设施

---

## 迁移的文件

### 从 grpc 模块移动到 alg 模块

| 文件 | 原路径 | 新路径 |
|------|--------|--------|
| 头文件 | `modules/grpc/include/grpc/video_grpc_client.h` | `modules/alg/include/alg/grpc/video_grpc_client.h` |
| 源文件 | `modules/grpc/src/video_grpc_client.cpp` | `modules/alg/src/grpc/video_grpc_client.cpp` |

---

## 修改的配置

### 1. modules/grpc/CMakeLists.txt

**删除的内容：**
```cmake
# 删除 video_processing proto 文件
- ${GRPC_GENERATED_DIR}/video_processing.pb.cc
- ${GRPC_GENERATED_DIR}/video_processing.grpc.pb.cc

# 删除 OpenCV 依赖
- find_package(OpenCV REQUIRED)
- ${OpenCV_LIBS}
```

**保留的内容：**
```cmake
# 只保留 hello proto 文件
set(GRPC_GENERATED_SOURCES
    ${GRPC_GENERATED_DIR}/hello.pb.cc
    ${GRPC_GENERATED_DIR}/hello.grpc.pb.cc
)

# 只链接通用依赖
target_link_libraries(grpc_lib
    PUBLIC
        gRPC::grpc++
        protobuf::libprotobuf
        log_lib
)
```

---

### 2. modules/alg/CMakeLists.txt

**新增的内容：**
```cmake
# 添加 video_processing proto 文件
set(GRPC_GENERATED_DIR ${CMAKE_SOURCE_DIR}/grpc/generated)
set(ALG_GRPC_GENERATED_SOURCES
    ${GRPC_GENERATED_DIR}/video_processing.pb.cc
    ${GRPC_GENERATED_DIR}/video_processing.grpc.pb.cc
)

add_library(alg_lib STATIC ${ALG_SOURCES} ${ALG_GRPC_GENERATED_SOURCES})

# 添加 generated 目录到 include 路径
target_include_directories(alg_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${GRPC_GENERATED_DIR}>  # ← 新增
        $<INSTALL_INTERFACE:include>
)
```

**依赖关系：**
```cmake
target_link_libraries(alg_lib
    PUBLIC
        ${OpenCV_LIBS}
        log_lib
        grpc_lib  # ← alg 依赖 grpc（正确的方向）
)
```

---

### 3. modules/grpc/test/CMakeLists.txt

**更新测试依赖：**
```cmake
target_link_libraries(test_grpc_video_client
    PRIVATE
        grpc_lib
        alg_lib  # ← 新增：video_grpc_client 在 alg 模块
        log_lib
)
```

---

### 4. 测试文件 Include 路径

**test_video_grpc_client.cpp：**
```cpp
// 修改前
#include "grpc/video_grpc_client.h"

// 修改后
#include "alg/grpc/video_grpc_client.h"
```

---

## 依赖关系图

### 修改前（错误）❌

```
grpc_lib
├── video_grpc_client (使用 OpenCV)
└── 依赖 alg_lib? (循环依赖！)

alg_lib
└── 被 grpc 依赖？
```

### 修改后（正确）✅

```
grpc_lib (通用基础设施)
├── grpc_client
├── grpc_server
└── hello_grpc_service
    ↓ 被依赖

alg_lib (算法相关)
├── i_algorithm_processor
├── grpc_to_alg
├── grpc_video_sender
├── video_grpc_client (使用 OpenCV)
└── 依赖 grpc_lib ✅
```

---

## 验证

### 1. 重新配置 CMake

```
Project → Delete Cache and Reconfigure
```

### 2. 编译

```
Build → Build All
```

应该看到：

```
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/src/grpc_client.cpp.obj
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/src/grpc_server.cpp.obj
[xx/xx] Linking CXX static library K:\...\modules\grpc\lib\grpc_lib.lib

[xx/xx] Building CXX object modules/alg/CMakeFiles/alg_lib.dir/src/grpc/grpc_to_alg.cpp.obj
[xx/xx] Building CXX object modules/alg/CMakeFiles/alg_lib.dir/src/grpc/video_grpc_client.cpp.obj
[xx/xx] Linking CXX static library K:\...\modules\alg\lib\alg_lib.lib
```

**注意：**
- grpc_lib **不再编译** video_grpc_client.cpp
- alg_lib **编译** video_grpc_client.cpp

---

## 优势

### 1. 清晰的依赖方向

```
alg_lib → grpc_lib ✅ (正确)
而不是
grpc_lib → alg_lib ❌ (错误)
```

### 2. 模块化更好

- **grpc_lib** 是纯通信层，可以被任何模块使用
- **alg_lib** 是业务层，依赖通信层

### 3. 避免循环依赖

之前可能存在：
```
grpc_lib 需要 alg_lib 的某些功能
alg_lib 需要 grpc_lib 的通信能力
→ 循环依赖！
```

现在：
```
alg_lib 单向依赖 grpc_lib
→ 没有循环依赖 ✅
```

### 4. 更易测试

- grpc_lib 可以独立测试（不需要 OpenCV）
- alg_lib 测试时只需要 mock grpc_lib

---

## 注意事项

### 1. Proto 文件分布

- `grpc/proto/hello.proto` → grpc 模块编译
- `grpc/proto/video_processing.proto` → alg 模块编译

两个模块都从同一个 `grpc/generated/` 目录读取生成的文件，但各自编译自己需要的部分。

### 2. Include 路径

- grpc 模块的头文件：`#include "grpc/xxx.h"`
- alg 模块的头文件：`#include "alg/grpc/xxx.h"`

### 3. 测试文件位置

测试文件仍在 `modules/grpc/test/`，但：
- 测试 grpc 功能的 → 只链接 grpc_lib
- 测试 alg 功能的 → 链接 grpc_lib + alg_lib

---

## 状态

✅ **video_grpc_client 已移到 alg 模块**
✅ **grpc 模块不再依赖 OpenCV**
✅ **alg 模块正确依赖 grpc 模块**
✅ **依赖方向正确：alg → grpc**
✅ **测试文件已更新**

请在 Visual Studio 中重新配置并编译项目！🎉
