# GRPC 测试文件 Include 路径修复

## 问题

测试文件迁移后，include 路径仍然使用旧的模块结构，导致编译错误：

```
fatal error C1083: 无法打开包括文件: "video_grpc_client.h": No such file or directory
fatal error C1083: 无法打开包括文件: "video_pipeline/algorithm_processor/i_algorithm_processor.h": No such file or directory
```

---

## 解决方案

更新所有测试文件的 include 路径和 namespace，使其符合新的模块化架构。

---

## 修改的文件

### 1. test_video_grpc_client.cpp

**修改前：**
```cpp
#include "video_grpc_client.h"
```

**修改后：**
```cpp
#include "grpc/video_grpc_client.h"
```

**原因：** grpc 模块的头文件现在在 `modules/grpc/include/grpc/` 目录。

---

### 2. test_grpc_algorithm_integration.cpp

**修改前：**
```cpp
#include "video_pipeline/algorithm_processor/i_algorithm_processor.h"
#include "video_pipeline/algorithm_processor/grpc_algorithm_processor.h"
using namespace video_pipeline::algorithm_processor;
```

**修改后：**
```cpp
#include "alg/grpc/i_algorithm_processor.h"
#include "alg/grpc/grpc_to_alg.h"
// using namespace video_pipeline::algorithm_processor;  // 旧的路径
// IAlgorithmProcessor 和 GrpcToAlg 在全局 namespace
```

**原因：** 
- 算法相关的 gRPC 实现已迁移到 alg 模块
- 类名从 `GrpcAlgorithmProcessor` 改为 `GrpcToAlg`
- 这些类在全局 namespace，不在 `video_pipeline::algorithm_processor` 中

---

### 3. test_video_processor.cpp

**修改前：**
```cpp
#include "video_pipeline/algorithm_processor/i_algorithm_processor.h"
#include "video_pipeline/algorithm_processor/grpc_algorithm_processor.h"
using namespace video_pipeline::algorithm_processor;
```

**修改后：**
```cpp
#include "alg/grpc/i_algorithm_processor.h"
#include "alg/grpc/grpc_to_alg.h"
// using namespace video_pipeline::algorithm_processor;  // 旧的路径
// IAlgorithmProcessor 和 GrpcToAlg 在全局 namespace
```

**原因：** 同上

---

### 4. modules/grpc/test/CMakeLists.txt

**修改：**
```cmake
target_link_libraries(test_grpc_video_processor
    PRIVATE
        grpc_lib
        alg_lib  # ← 新增：需要 alg_lib 的 IAlgorithmProcessor
        log_lib
)
```

**原因：** test_video_processor 使用了 alg 模块的接口，需要链接 alg_lib。

---

## Include 路径映射表

| 旧路径 | 新路径 | 说明 |
|--------|--------|------|
| `video_grpc_client.h` | `grpc/video_grpc_client.h` | grpc 模块头文件 |
| `video_pipeline/algorithm_processor/i_algorithm_processor.h` | `alg/grpc/i_algorithm_processor.h` | alg 模块的算法处理器接口 |
| `video_pipeline/algorithm_processor/grpc_algorithm_processor.h` | `alg/grpc/grpc_to_alg.h` | alg 模块的 gRPC 算法处理器实现 |

---

## Namespace 变化

| 旧 Namespace | 新 Namespace | 说明 |
|--------------|--------------|------|
| `video_pipeline::algorithm_processor` | 全局 namespace | IAlgorithmProcessor 和 GrpcToAlg 现在在全局 namespace |

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
[xx/xx] Building CXX object modules/grpc/test/CMakeFiles/test_grpc_video_client.dir/test_video_grpc_client.cpp.obj
[xx/xx] Building CXX object modules/grpc/test/CMakeFiles/test_grpc_algorithm_integration.dir/test_grpc_algorithm_integration.cpp.obj
[xx/xx] Building CXX object modules/grpc/test/CMakeFiles/test_grpc_video_processor.dir/test_video_processor.cpp.obj
```

成功编译！✅

---

## 注意事项

### 1. 类名变化

- ❌ 旧：`GrpcAlgorithmProcessor`
- ✅ 新：`GrpcToAlg`

如果测试代码中使用了类名，需要相应更新。

### 2. 依赖关系

测试可执行文件的依赖：

```
test_grpc_video_client
├── grpc_lib
└── log_lib

test_grpc_algorithm_integration
├── grpc_lib
├── alg_lib      # ← 需要 alg 模块
└── log_lib

test_grpc_video_processor
├── grpc_lib
├── alg_lib      # ← 需要 alg 模块
└── log_lib
```

### 3. 其他测试文件

如果还有其他测试文件使用了旧的 include 路径，需要类似地更新。

---

## 状态

✅ **test_video_grpc_client.cpp - include 路径已更新**
✅ **test_grpc_algorithm_integration.cpp - include 路径和 namespace 已更新**
✅ **test_video_processor.cpp - include 路径和 namespace 已更新**
✅ **CMakeLists.txt - 依赖已添加**

请在 Visual Studio 中重新编译项目！🎉
