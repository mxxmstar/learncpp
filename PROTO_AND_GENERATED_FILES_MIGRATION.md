# Proto 和 Generated 文件模块化迁移完成

## 概述

将 proto 源文件和生成的 `.pb.*` 文件按照模块职责进行分离，实现完全的模块化。

---

## 文件分布

### GRPC 模块（通用基础设施）

**Proto 源文件：**
- ✅ `grpc/proto/hello.proto` - Hello 服务示例

**Generated 文件：**
- ✅ `modules/grpc/generated/hello.pb.h`
- ✅ `modules/grpc/generated/hello.pb.cc`
- ✅ `modules/grpc/generated/hello.grpc.pb.h`
- ✅ `modules/grpc/generated/hello.grpc.pb.cc`

**特点：**
- 通用的 gRPC 示例
- 不依赖业务逻辑
- 可被任何模块复用

---

### ALG 模块（算法相关）

**Proto 源文件：**
- ✅ `modules/alg/proto/video_processing.proto` - 视频处理 proto ← **从 grpc 移到这里**

**Generated 文件：**
- ✅ `modules/alg/generated/video_processing.pb.h` ← **从 grpc 移到这里**
- ✅ `modules/alg/generated/video_processing.pb.cc`
- ✅ `modules/alg/generated/video_processing.grpc.pb.h`
- ✅ `modules/alg/generated/video_processing.grpc.pb.cc`

**特点：**
- 算法相关的 gRPC 定义
- 被 alg 模块编译和使用
- 与 video_grpc_client、GrpcToAlg 等配套

---

## 迁移的文件

### 从 grpc 移动到 alg

| 文件类型 | 原路径 | 新路径 |
|---------|--------|--------|
| Proto 源文件 | `grpc/proto/video_processing.proto` | `modules/alg/proto/video_processing.proto` |
| Generated | `grpc/generated/video_processing.pb.h` | `modules/alg/generated/video_processing.pb.h` |
| Generated | `grpc/generated/video_processing.pb.cc` | `modules/alg/generated/video_processing.pb.cc` |
| Generated | `grpc/generated/video_processing.grpc.pb.h` | `modules/alg/generated/video_processing.grpc.pb.h` |
| Generated | `grpc/generated/video_processing.grpc.pb.cc` | `modules/alg/generated/video_processing.grpc.pb.cc` |

---

## 目录结构

### 迁移前 ❌

```
grpc/
├── proto/
│   ├── hello.proto
│   └── video_processing.proto    ← 混在一起
└── generated/
    ├── hello.pb.*
    └── video_processing.pb.*     ← 混在一起

modules/
├── grpc/
│   └── (没有 proto 或 generated)
└── alg/
    └── (没有 proto 或 generated)
```

### 迁移后 ✅

```
grpc/                              ← 原目录，仅保留 hello.proto
├── proto/
│   └── hello.proto                ← grpc 模块的 proto
└── generated/
    └── hello.pb.*                 ← grpc 模块的 generated

modules/
├── grpc/                          ← grpc 模块
│   ├── include/grpc/
│   ├── src/
│   └── generated/                 ← grpc 模块的 generated
│       ├── hello.pb.h
│       ├── hello.pb.cc
│       ├── hello.grpc.pb.h
│       └── hello.grpc.pb.cc
│
└── alg/                           ← alg 模块
    ├── include/alg/grpc/
    ├── src/grpc/
    ├── proto/                     ← alg 模块的 proto
    │   └── video_processing.proto
    └── generated/                 ← alg 模块的 generated
        ├── video_processing.pb.h
        ├── video_processing.pb.cc
        ├── video_processing.grpc.pb.h
        └── video_processing.grpc.pb.cc
```

---

## 修改的配置

### 1. modules/alg/CMakeLists.txt

**修改前：**
```cmake
set(GRPC_GENERATED_DIR ${CMAKE_SOURCE_DIR}/grpc/generated)
set(ALG_GRPC_GENERATED_SOURCES
    ${GRPC_GENERATED_DIR}/video_processing.pb.cc
    ${GRPC_GENERATED_DIR}/video_processing.grpc.pb.cc
)

target_include_directories(alg_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${GRPC_GENERATED_DIR}>
        $<INSTALL_INTERFACE:include>
)
```

**修改后：**
```cmake
set(ALG_GRPC_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)
set(ALG_GRPC_GENERATED_SOURCES
    ${ALG_GRPC_GENERATED_DIR}/video_processing.pb.cc
    ${ALG_GRPC_GENERATED_DIR}/video_processing.grpc.pb.cc
)

target_include_directories(alg_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${ALG_GRPC_GENERATED_DIR}>  # ← 使用 alg 自己的 generated
        $<INSTALL_INTERFACE:include>
)
```

---

### 2. generate_grpc_code.ps1

**修改前：**
```powershell
$outputDir = "modules/grpc/generated"

& $PROTOC `
  --proto_path=grpc/proto `
  --cpp_out=$outputDir `
  --grpc_out=$outputDir `
  --plugin=protoc-gen-grpc=$PLUGIN `
  grpc/proto/hello.proto `
  grpc/proto/video_processing.proto
```

**修改后：**
```powershell
$grpcOutputDir = "modules/grpc/generated"
$algOutputDir = "modules/alg/generated"

# 1. 生成 hello.proto → grpc 模块
& $PROTOC `
  --proto_path=grpc/proto `
  --cpp_out=$grpcOutputDir `
  --grpc_out=$grpcOutputDir `
  --plugin=protoc-gen-grpc=$PLUGIN `
  grpc/proto/hello.proto

# 2. 生成 video_processing.proto → alg 模块
& $PROTOC `
  --proto_path=modules/alg/proto `
  --cpp_out=$algOutputDir `
  --grpc_out=$algOutputDir `
  --plugin=protoc-gen-grpc=$PLUGIN `
  modules/alg/proto/video_processing.proto
```

---

## 重新生成代码

运行脚本会分别生成到两个模块：

```powershell
.\generate_grpc_code.ps1
```

**输出示例：**
```
Protoc version:
libprotoc 29.5

[1/2] Generating hello.proto...

[2/2] Generating video_processing.proto...

Code generation successful!

Generated files in grpc module:
  - hello.grpc.pb.cc
  - hello.grpc.pb.h
  - hello.pb.cc
  - hello.pb.h

Generated files in alg module:
  - video_processing.grpc.pb.cc
  - video_processing.grpc.pb.h
  - video_processing.pb.cc
  - video_processing.pb.h
```

---

## 优势

### 1. 完全模块化

每个模块都有自己的：
- ✅ Proto 源文件（接口定义）
- ✅ Generated 文件（实现代码）
- ✅ 头文件和源文件
- ✅ 测试文件

### 2. 清晰的职责

**grpc 模块：**
- 提供通用 gRPC 基础设施
- hello.proto 作为示例

**alg 模块：**
- 实现算法相关的 gRPC 通信
- video_processing.proto 定义算法接口

### 3. 独立编译

- grpc 模块只编译 hello.pb.*
- alg 模块只编译 video_processing.pb.*
- 互不干扰

### 4. 易于维护

- 修改 video_processing.proto → 只影响 alg 模块
- 添加新的 proto → 放到对应模块
- 删除模块 → 所有相关文件一起删除

### 5. 便于复用

如果将来有其他模块需要使用 video_processing.proto：
- 可以直接引用 alg 模块
- 或者复制 proto 到自己的模块
- 不会造成混乱

---

## 注意事项

### 1. Proto 文件位置

- **grpc/proto/** - grpc 模块的 proto
- **modules/alg/proto/** - alg 模块的 proto

每个模块管理自己的 proto 文件。

### 2. Generated 文件位置

- **modules/grpc/generated/** - grpc 模块的 generated
- **modules/alg/generated/** - alg 模块的 generated

Generated 文件与模块源码放在一起。

### 3. 原来的 grpc/ 目录

原来的 `grpc/` 目录仍然保留，但：
- `grpc/proto/hello.proto` - 仍在使用（grpc 模块）
- `grpc/proto/video_processing.proto` - 已移到 alg
- `grpc/generated/*` - hello 的仍在，video_processing 的已移到 alg
- `grpc/include/`, `grpc/src/` - 已废弃，使用 modules/grpc/

建议在未来清理旧的 grpc 目录。

### 4. CMake 配置

确保每个模块的 CMakeLists.txt 正确引用自己的 generated 目录：

```cmake
# grpc 模块
set(GRPC_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)

# alg 模块
set(ALG_GRPC_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)
```

---

## 验证

### 1. 重新生成代码

```powershell
.\generate_grpc_code.ps1
```

确认文件生成到正确的目录。

### 2. 重新配置 CMake

```
Project → Delete Cache and Reconfigure
```

### 3. 编译

```
Build → Build All
```

应该看到：

```
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/generated/hello.pb.cc.obj
[xx/xx] Building CXX object modules/alg/CMakeFiles/alg_lib.dir/generated/video_processing.pb.cc.obj
```

成功编译！✅

---

## 状态

✅ **video_processing.proto 已移到 modules/alg/proto/**
✅ **video_processing.pb.* 已移到 modules/alg/generated/**
✅ **alg CMakeLists.txt 已更新路径**
✅ **generate_grpc_code.ps1 已更新，分别生成到两个模块**
✅ **完全模块化完成**

请在 Visual Studio 中重新配置并编译项目！🎉
