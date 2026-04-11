# 清理和重新编译指南

## ⚠️ 问题说明

如果你发现 CMake 仍然在编译 `src/` 目录下的旧文件，这是因为 **CMake 缓存**保留了旧的配置。

## 🔧 解决方案

### 方法 1：使用自动化脚本（推荐）

```powershell
.\clean_and_rebuild.ps1
```

这个脚本会：
1. 删除旧的 build 目录
2. 创建新的 build 目录
3. 重新配置 CMake
4. 编译项目
5. 显示编译的文件列表

### 方法 2：手动清理

```powershell
# 1. 删除 build 目录
Remove-Item -Recurse -Force build

# 2. 创建新的 build 目录
mkdir build
cd build

# 3. 重新配置 CMake
cmake ..

# 4. 编译
cmake --build . --config Debug
```

## ✅ 验证

编译完成后，应该只看到以下文件被编译：

### 源文件
- ✓ `apps/main.cpp`
- ✓ `modules/log/src/logger.cpp`
- ✓ `modules/log/src/logmanager.cpp`

### 输出文件
- ✓ `bin/MySelfContainedApp.exe`
- ✓ `modules/log/lib/log_lib.lib`

### 不应该编译的文件
- ✗ `src/net/*.cpp`
- ✗ `src/puller/*.cpp`
- ✗ `src/decoder/*.cpp`
- ✗ `src/video_pipeline/*.cpp`
- ✗ 其他 `src/` 目录下的文件

## 🔍 检查编译范围

编译完成后，你可以检查 CMake 生成的文件列表：

```powershell
# 查看编译的源文件
Get-Content build/CMakeFiles/MySelfContainedApp.dir/link.txt

# 或者查看 ninja/build.ninja 文件
Get-Content build/build.ninja | Select-String "apps/main.cpp"
Get-Content build/build.ninja | Select-String "modules/log"
```

## 💡 为什么需要清理缓存？

CMake 会缓存以下信息：
- 扫描到的源文件列表（GLOB_RECURSE 的结果）
- 依赖关系
- 编译选项

当你修改 CMakeLists.txt 后，**必须清除缓存**才能让新配置生效。

## 📝 注意事项

1. **每次修改 CMakeLists.txt 后**，建议清理并重新配置
2. **不要手动编辑** build 目录下的文件
3. **保留 build 目录**可以加快增量编译速度（在不修改 CMakeLists.txt 的情况下）
