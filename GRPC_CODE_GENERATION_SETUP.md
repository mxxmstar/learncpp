# GRPC 代码生成配置更新

## 概述

统一使用 `tools/win32/protobuf/protoc.exe` 生成 gRPC 代码，确保生成的代码与 vcpkg 安装的 Protobuf 库版本兼容。

---

## 配置详情

### Protoc 路径

**之前：**
```powershell
$PROTOC = "out/build/x64-Debug/vcpkg_installed/x64-windows/tools/protobuf/protoc.exe"
```

**现在：**
```powershell
$PROTOC = "tools/win32/protobuf/protoc.exe"
```

### 版本信息

- **Protoc 版本**: libprotoc 29.5
- **vcpkg Protobuf 库版本**: 29.5.0
- **生成的代码要求**: PROTOBUF_VERSION == 5029005 (29.5)

✅ **版本完全匹配！**

---

## 修改的文件

### `generate_grpc_code.ps1`

**主要变更：**

1. ✅ 使用 `tools/win32/protobuf/protoc.exe`
2. ✅ 显示 protoc 版本信息
3. ✅ 移除自动禁用版本检查的代码
4. ✅ 保持简洁，只负责生成代码

**删除的功能：**
- ❌ 不再自动注释版本检查
- ❌ 不再使用正则表达式修改生成的文件

**原因：**
- 现在 protoc 版本与 vcpkg 库版本一致
- 不需要禁用版本检查
- 生成的代码可以直接编译

---

## 目录结构

```
tools/win32/protobuf/          ← 统一的 protoc 工具
├── protoc.exe                  (libprotoc 29.5)
└── ...

modules/grpc/generated/         ← gRPC 生成的代码
├── hello.pb.cc
├── hello.pb.h
├── hello.grpc.pb.cc
├── hello.grpc.pb.h
├── video_processing.pb.cc
├── video_processing.pb.h
├── video_processing.grpc.pb.cc
└── video_processing.grpc.pb.h

grpc/proto/                     ← Proto 源文件
├── hello.proto
└── video_processing.proto
```

---

## 重新生成代码

### 清理旧文件

```powershell
Remove-Item -Path "modules\grpc\generated" -Recurse -Force
New-Item -ItemType Directory -Path "modules\grpc\generated" -Force
```

### 运行生成脚本

```powershell
.\generate_grpc_code.ps1
```

**输出示例：**
```
Protoc: tools/win32/protobuf/protoc.exe
Plugin: out/build/x64-Debug/vcpkg_installed/x64-windows/tools/grpc/grpc_cpp_plugin.exe

Protoc version:
libprotoc 29.5

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
```

---

## 验证

### 1. 检查生成的版本要求

```powershell
Get-Content "modules\grpc\generated\hello.pb.h" | Select-String "PROTOBUF_VERSION" | Select-Object -First 1
```

应该看到：
```cpp
#if PROTOBUF_VERSION != 5029005
```

### 2. 检查 vcpkg 库版本

```powershell
Get-Content "out\build\x64-debug\vcpkg_installed\x64-windows\share\protobuf\protobuf-config-version.cmake" | Select-String "PACKAGE_VERSION" | Select-Object -First 1
```

应该看到：
```cmake
set(PACKAGE_VERSION "29.5.0")
```

### 3. 编译项目

```
Build → Build All
```

应该成功编译 grpc_lib，没有版本检查错误。

---

## 优势

### 1. 版本一致性
- ✅ protoc 版本 = 29.5
- ✅ vcpkg 库版本 = 29.5.0
- ✅ 生成的代码要求 = 29.5
- ✅ **完全匹配，无需 workaround**

### 2. 统一管理
- ✅ protoc 放在 `tools/win32/` 目录
- ✅ 所有开发者使用相同版本
- ✅ 不依赖 vcpkg 的 protoc

### 3. 简化流程
- ✅ 生成脚本更简单
- ✅ 不需要后处理步骤
- ✅ 减少出错可能

### 4. 清晰明确
- ✅ 版本要求清晰可见
- ✅ 如果版本不匹配会立即报错
- ✅ 便于排查问题

---

## 注意事项

### 1. 更新 Protobuf 版本时

如果需要升级 Protobuf 版本：

1. 更新 `tools/win32/protobuf/protoc.exe`
2. 更新 vcpkg protobuf 包
3. 重新生成 gRPC 代码
4. 确保版本匹配

### 2. 团队协作

确保团队成员都使用相同版本的 protoc：

```powershell
# 提交 tools/win32/protobuf 到版本控制
git add tools/win32/protobuf/
git commit -m "Add protoc 29.5 for consistent gRPC code generation"
```

### 3. CI/CD

在 CI/CD 环境中也需要使用相同版本的 protoc：

```yaml
# 示例：GitHub Actions
- name: Setup protoc
  run: |
    cp tools/win32/protobuf/protoc.exe /usr/local/bin/
    chmod +x /usr/local/bin/protoc
```

---

## 状态

✅ **使用 tools/win32/protobuf/protoc.exe**
✅ **版本完全匹配 (29.5)**
✅ **无需禁用版本检查**
✅ **可以正常编译**

请在 Visual Studio 中重新编译项目！🎉
