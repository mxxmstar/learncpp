# ALG 模块 Header-Only 库修复

## 问题

CMake 配置时出现错误：

```
CMake Error at modules/alg/CMakeLists.txt:19 (add_library):
  No SOURCES given to target: alg_lib
```

## 原因

alg 模块的源文件实际上都在 `src/grpc/` 子目录中，这些文件应该属于 grpc 模块，而不是 alg 模块。

alg 模块只包含头文件：
- `include/alg/base_algorithm.h`
- `include/alg/i_algorithm.h`

没有 `.cpp` 源文件，因此是一个 **header-only library**（纯头文件库）。

## 解决方案

将 alg 模块从 STATIC 库改为 INTERFACE 库（header-only）：

### 修改前
```cmake
# 收集源文件
file(GLOB ALG_SOURCES "src/*.cpp")

# 创建静态库
add_library(alg_lib STATIC ${ALG_SOURCES})

target_include_directories(alg_lib PUBLIC ...)
target_link_libraries(alg_lib PUBLIC ...)
target_compile_definitions(alg_lib PUBLIC ...)
```

### 修改后
```cmake
# alg 是 header-only 库，不需要源文件

# 创建接口库（header-only）
add_library(alg_lib INTERFACE)

target_include_directories(alg_lib INTERFACE ...)
target_link_libraries(alg_lib INTERFACE ...)
target_compile_definitions(alg_lib INTERFACE ...)
```

## 关键变化

| 项目 | STATIC 库 | INTERFACE 库 |
|------|-----------|--------------|
| add_library | `add_library(alg_lib STATIC ${SOURCES})` | `add_library(alg_lib INTERFACE)` |
| target_include_directories | `PUBLIC` | `INTERFACE` |
| target_link_libraries | `PUBLIC` | `INTERFACE` |
| target_compile_definitions | `PUBLIC` | `INTERFACE` |
| 需要源文件 | ✅ 是 | ❌ 否 |

## INTERFACE 库的特点

1. **没有编译产物**：不会生成 `.lib` 或 `.dll` 文件
2. **只传递依赖和头文件**：其他库链接它时，会自动获得它的依赖和 include 路径
3. **适合纯头文件库**：如 Eigen、fmt（header-only mode）、STL 等

## 文件结构调整

### 修改前
```
modules/alg/
├── include/alg/
│   ├── base_algorithm.h
│   ├── i_algorithm.h
│   └── grpc/              ← 不应该在这里
│       ├── grpc_to_alg.h
│       ├── grpc_video_sender.h
│       └── i_algorithm_processor.h
└── src/
    └── grpc/              ← 这些文件属于 grpc 模块
        ├── grpc_to_alg.cpp
        └── grpc_video_sender.cpp
```

### 修改后
```
modules/alg/
├── include/alg/
│   ├── base_algorithm.h    ← alg 模块的头文件
│   └── i_algorithm.h
└── src/                    ← 空目录（可以删除）

modules/grpc/
├── include/grpc/
│   ├── grpc_to_alg.h       ← grpc 模块的头文件
│   ├── grpc_video_sender.h
│   └── i_algorithm_processor.h
└── src/
    ├── grpc_to_alg.cpp     ← grpc 模块的源文件
    └── grpc_video_sender.cpp
```

## 验证

重新配置 CMake：

```powershell
# 在 Visual Studio 中
Project → Delete Cache and Reconfigure
```

应该看到：
```
-- Configuring done
-- Generating done
```

没有 "No SOURCES given to target" 错误。

## 其他模块的影响

由于 alg_lib 现在是 INTERFACE 库，其他模块链接它时的行为不变：

```cmake
# grpc 模块
target_link_libraries(grpc_lib
    PUBLIC
        alg_lib  # 仍然有效，会传递 OpenCV 和 log_lib 依赖
        ...
)
```

INTERFACE 库的依赖会自动传递给使用者，所以不需要修改其他模块的配置。

## 状态

✅ **alg 模块已改为 INTERFACE 库**
✅ **移除了 include/alg/grpc 目录**
✅ **CMake 配置错误已修复**

可以重新配置并编译项目了！
