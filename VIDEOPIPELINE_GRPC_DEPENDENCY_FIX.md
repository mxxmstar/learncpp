# VideoPipeline 和 GRPC 模块依赖修复

## 问题 1：VideoPipeline 缺少 puller_lib 和 grpc_lib 依赖

### 错误信息
```
fatal error C1083: 无法打开包括文件: "puller/zlm/zlm_httpflv_puller.h": No such file or directory
```

### 原因
`video_pipeline.h` 包含了 `puller/zlm/zlm_httpflv_puller.h`，但 videopipeline 模块的 CMakeLists.txt 中没有链接 `puller_lib`。

### 解决方案
在 `modules/videopipeline/CMakeLists.txt` 中添加依赖：

```cmake
target_link_libraries(videopipeline_lib
    PUBLIC
        ${OpenCV_LIBS}
        decoder_lib
        preprocess_lib
        postprocess_lib
        alg_lib
        grpc_lib      # ← 新增
        puller_lib    # ← 新增
        log_lib
        net_lib
        Boost::asio
        Boost::lockfree
)
```

---

## 问题 2：GRPC 模块头文件路径错误

### 错误信息
```
fatal error C1083: 无法打开包括文件: "alg/grpc/grpc_to_alg.h": No such file or directory
fatal error C1083: 无法打开包括文件: "alg/grpc/grpc_video_sender.h": No such file or directory
```

### 原因
grpc 模块的源文件中使用了错误的 include 路径：
- ❌ `#include "alg/grpc/grpc_to_alg.h"`
- ✅ `#include "grpc/grpc_video_sender.h"`

grpc 模块的头文件在 `modules/grpc/include/grpc/` 目录下，不是 `alg/grpc/`。

### 解决方案

修改以下文件的 include 路径：

#### 1. `modules/grpc/src/grpc_to_alg.cpp`
```cpp
// 修改前
#include "alg/grpc/grpc_to_alg.h"

// 修改后
#include "grpc/grpc_to_alg.h"
```

#### 2. `modules/grpc/src/grpc_video_sender.cpp`
```cpp
// 修改前
#include "alg/grpc/grpc_video_sender.h"

// 修改后
#include "grpc/grpc_video_sender.h"
```

#### 3. `modules/videopipeline/include/videopipeline/video_pipeline.h`
```cpp
// 修改前
#include "alg/grpc/grpc_video_sender.h"

// 修改后
#include "grpc/grpc_video_sender.h"
```

---

## 修改的文件清单

1. ✅ `modules/videopipeline/CMakeLists.txt` - 添加 grpc_lib 和 puller_lib 依赖
2. ✅ `modules/grpc/src/grpc_to_alg.cpp` - 修正 include 路径
3. ✅ `modules/grpc/src/grpc_video_sender.cpp` - 修正 include 路径
4. ✅ `modules/videopipeline/include/videopipeline/video_pipeline.h` - 修正 include 路径

---

## 模块依赖关系

修复后的完整依赖链：

```
videopipeline_lib
├── puller_lib          # ← 新增
│   ├── Boost::asio
│   ├── Boost::beast
│   └── log_lib
├── grpc_lib            # ← 新增
│   ├── alg_lib (INTERFACE)
│   ├── gRPC::grpc++
│   ├── protobuf::libprotobuf
│   └── log_lib
├── decoder_lib
├── preprocess_lib
├── postprocess_lib
├── alg_lib (INTERFACE)
├── log_lib
├── net_lib
├── OpenCV
├── Boost::asio
└── Boost::lockfree
```

---

## 验证

重新配置并编译：

```powershell
# 在 Visual Studio 中
Project → Delete Cache and Reconfigure
Build → Build All
```

应该能成功编译 videopipeline_lib 和 grpc_lib。

---

## 注意事项

### Include 路径规范

所有模块的 include 路径应该遵循以下规范：

```cpp
// ✅ 正确：使用模块名作为前缀
#include "grpc/grpc_to_alg.h"
#include "puller/zlm/zlm_httpflv_puller.h"
#include "decoder/ffmpeg_decoder.h"

// ❌ 错误：不要混用模块名
#include "alg/grpc/grpc_to_alg.h"  // grpc 不属于 alg
```

### 模块独立性

- **grpc 模块**是独立的，不属于 alg 模块
- alg 模块只包含算法接口定义（header-only）
- grpc 模块实现基于 gRPC 的算法处理器

---

## 状态

✅ **videopipeline 依赖已修复**
✅ **grpc 头文件路径已修正**
✅ **所有 include 路径已统一**

可以重新编译项目了！🎉
