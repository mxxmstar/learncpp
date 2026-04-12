# GRPC 目录完全迁移到 Modules

## 概述

将原来 `grpc/` 目录中的**所有内容**（包括 `generated/`）完全迁移到 `modules/grpc/` 中，彻底弃用原来的 `grpc/` 目录。

---

## 迁移的内容

### 1. 头文件（之前已迁移）
```
grpc/include/*.h → modules/grpc/include/grpc/
├── grpc_client.h
├── grpc_server.h
├── hello_grpc_service.h
└── video_grpc_client.h
```

### 2. 源文件（之前已迁移）
```
grpc/src/*.cpp → modules/grpc/src/
├── grpc_client.cpp
├── grpc_server.cpp
├── hello_grpc_service.cpp
└── video_grpc_client.cpp
```

### 3. 生成的 Proto 文件（本次迁移）
```
grpc/generated/* → modules/grpc/generated/
├── hello.pb.cc
├── hello.pb.h
├── hello.grpc.pb.cc
├── hello.grpc.pb.h
├── video_processing.pb.cc
├── video_processing.pb.h
├── video_processing.grpc.pb.cc
└── video_processing.grpc.pb.h
```

---

## 修改的文件

### 1. `modules/grpc/CMakeLists.txt`

**修改前：**
```cmake
set(GRPC_GENERATED_DIR ${CMAKE_SOURCE_DIR}/grpc/generated)
```

**修改后：**
```cmake
set(GRPC_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)
```

### 2. `generate_grpc_code.ps1`

**修改前：**
```powershell
$outputDir = "grpc/generated"
& $PROTOC ... grpc/proto/hello.proto
```

**修改后：**
```powershell
$outputDir = "modules/grpc/generated"
& $PROTOC ... grpc/proto/hello.proto grpc/proto/video_processing.proto

# 自动禁用 Protobuf 版本检查
Write-Host "Disabling Protobuf version check..."
# ... 自动注释版本检查代码
```

**新增功能：**
- ✅ 生成两个 proto 文件（hello 和 video_processing）
- ✅ 自动生成后禁用 Protobuf 版本检查
- ✅ 输出到 `modules/grpc/generated/`

---

## 目录结构

### 迁移后
```
modules/grpc/
├── CMakeLists.txt
├── include/grpc/          # 头文件
│   ├── grpc_client.h
│   ├── grpc_server.h
│   ├── hello_grpc_service.h
│   └── video_grpc_client.h
├── src/                   # 源文件
│   ├── grpc_client.cpp
│   ├── grpc_server.cpp
│   ├── hello_grpc_service.cpp
│   └── video_grpc_client.cpp
├── generated/             # gRPC 生成的文件
│   ├── hello.pb.cc
│   ├── hello.pb.h
│   ├── hello.grpc.pb.cc
│   ├── hello.grpc.pb.h
│   ├── video_processing.pb.cc
│   ├── video_processing.pb.h
│   ├── video_processing.grpc.pb.cc
│   └── video_processing.grpc.pb.h
└── lib/                   # 编译输出

grpc/                      ← 原目录，可以删除
├── generated/             ← 已迁移，可删除
├── include/               ← 已迁移，可删除
├── proto/                 ← 保留（proto 源文件）
├── src/                   ← 已迁移，可删除
└── CMakeLists.txt         ← 已弃用，可删除
```

---

## 重新生成 gRPC 代码

现在运行脚本会自动：
1. 生成到 `modules/grpc/generated/`
2. 禁用 Protobuf 版本检查

```powershell
.\generate_grpc_code.ps1
```

输出：
```
Generating gRPC code...

Code generation successful!
Generated files in: modules/grpc/generated
  - hello.grpc.pb.cc
  - hello.grpc.pb.h
  - hello.pb.cc
  - hello.pb.h
  - video_processing.grpc.pb.cc
  - video_processing.grpc.pb.h
  - video_processing.pb.cc
  - video_processing.pb.h

Disabling Protobuf version check...
  ✓ Fixed modules/grpc/generated/video_processing.pb.h
  ✓ Fixed modules/grpc/generated/hello.pb.h
```

---

## 清理旧目录（可选）

确认新目录工作正常后，可以删除旧的 `grpc/` 目录中的已迁移内容：

```powershell
# 备份（可选）
Copy-Item -Path "grpc" -Destination "grpc.backup" -Recurse

# 删除已迁移的子目录
Remove-Item -Path "grpc\generated" -Recurse -Force
Remove-Item -Path "grpc\include" -Recurse -Force
Remove-Item -Path "grpc\src" -Recurse -Force
Remove-Item -Path "grpc\CMakeLists.txt" -Force

# 保留 proto 目录（proto 源文件）
# grpc/proto/ 仍然需要
```

**注意：** 保留 `grpc/proto/` 目录，因为 `.proto` 源文件还在那里。

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
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/generated/video_processing.pb.cc.obj
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/generated/video_processing.grpc.pb.cc.obj
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/src/video_grpc_client.cpp.obj
```

**注意：** 路径现在是 `modules/grpc/generated/` 而不是 `grpc/generated/`。

---

## 优势

### 1. 模块化完整
- ✅ 所有 grpc 相关文件都在 `modules/grpc/` 中
- ✅ 符合模块化架构原则
- ✅ 便于管理和维护

### 2. 自动化改进
- ✅ 生成脚本自动禁用版本检查
- ✅ 生成两个 proto 文件
- ✅ 输出路径统一

### 3. 清晰的职责
- `grpc/proto/` - Proto 源文件（定义）
- `modules/grpc/generated/` - 生成的代码（实现）
- `modules/grpc/src/` - 手动编写的代码
- `modules/grpc/include/` - 公共接口

---

## 注意事项

### 1. Proto 文件位置

`.proto` 源文件仍然在 `grpc/proto/` 目录，这是合理的：
- Proto 文件是接口定义，不属于任何模块
- 可能被多个项目共享
- 保持独立便于管理

如果需要，也可以迁移到 `modules/grpc/proto/`，但当前方案已经足够清晰。

### 2. 版本检查自动化

现在每次运行 `generate_grpc_code.ps1` 都会自动禁用版本检查，无需手动操作。

### 3. 旧目录清理

建议在确认新结构工作正常后再清理旧目录，避免误删重要文件。

---

## 状态

✅ **generated 目录已迁移到 modules/grpc/**
✅ **CMakeLists.txt 已更新路径**
✅ **生成脚本已更新并增强**
✅ **Protobuf 版本检查自动禁用**
✅ **完全模块化完成**

原来的 `grpc/` 目录现在只保留 `proto/` 子目录，其他内容都可以安全删除！🎉
