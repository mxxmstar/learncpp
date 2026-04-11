# 模块测试开关说明

## 📋 概述

每个模块都有独立的测试开关，可以灵活控制是否编译该模块的测试。

## 🔧 可用的测试开关

### Log 模块
```cmake
option(BUILD_LOG_TESTS "Build log module tests" ON)
```

**默认值**: `ON`（启用）

**使用方法**:
```powershell
# 启用 log 测试（默认）
cmake .. -DBUILD_LOG_TESTS=ON

# 禁用 log 测试
cmake .. -DBUILD_LOG_TESTS=OFF
```

### Net 模块
```cmake
option(BUILD_NET_TESTS "Build net module tests" ON)
```

**默认值**: `ON`（启用）

**使用方法**:
```powershell
# 启用 net 测试（默认）
cmake .. -DBUILD_NET_TESTS=ON

# 禁用 net 测试
cmake .. -DBUILD_NET_TESTS=OFF
```

## 💡 使用场景

### 场景 1：快速编译（跳过测试）

当你只需要编译库文件，不需要运行测试时：

```powershell
cd build
cmake .. -DBUILD_LOG_TESTS=OFF -DBUILD_NET_TESTS=OFF
cmake --build . --config Debug
```

**优点**:
- ⚡ 编译速度更快
- 📦 只生成库文件（.lib）
- 🎯 专注于核心功能开发

### 场景 2：完整编译（包含测试）

当你需要运行测试验证功能时：

```powershell
cd build
cmake .. -DBUILD_LOG_TESTS=ON -DBUILD_NET_TESTS=ON
cmake --build . --config Debug
```

**优点**:
- ✅ 生成测试可执行文件
- 🧪 可以运行单元测试
- 🔍 便于调试和验证

### 场景 3：单独测试某个模块

当你只关心某个模块的测试时：

```powershell
# 只编译 log 模块的测试
cmake .. -DBUILD_LOG_TESTS=ON -DBUILD_NET_TESTS=OFF

# 只编译 net 模块的测试
cmake .. -DBUILD_LOG_TESTS=OFF -DBUILD_NET_TESTS=ON
```

## 📊 编译输出对比

### 启用测试（BUILD_XXX_TESTS=ON）

```
modules/log/
├── lib/
│   └── log_lib.lib              # 库文件
└── test/
    └── bin/
        ├── test_log_logger.exe  # 测试可执行文件
        └── test_log_logmanage.exe

modules/net/
├── lib/
│   └── net_lib.lib              # 库文件
└── test/
    └── bin/
        ├── test_net_httpserver.exe
        ├── test_net_tcpserver.exe
        └── ...
```

### 禁用测试（BUILD_XXX_TESTS=OFF）

```
modules/log/
└── lib/
    └── log_lib.lib              # 只有库文件

modules/net/
└── lib/
    └── net_lib.lib              # 只有库文件
```

## 🎯 最佳实践

### 开发阶段
```powershell
# 快速迭代，跳过测试
cmake .. -DBUILD_LOG_TESTS=OFF -DBUILD_NET_TESTS=OFF
```

### 测试阶段
```powershell
# 完整编译，运行测试
cmake .. -DBUILD_LOG_TESTS=ON -DBUILD_NET_TESTS=ON
ctest -C Debug --verbose
```

### CI/CD 环境
```powershell
# 根据环境变量动态控制
cmake .. -DBUILD_LOG_TESTS=$Env:RUN_TESTS -DBUILD_NET_TESTS=$Env:RUN_TESTS
```

## 📝 注意事项

1. **默认启用测试**: 所有模块的测试开关默认为 `ON`
2. **独立控制**: 每个模块的测试开关互不影响
3. **缓存机制**: 修改开关后需要重新配置 CMake（清理 build 目录）
4. **enable_testing()**: 只有在启用测试时才会调用 `enable_testing()`

## 🔍 查看当前配置

```powershell
# 查看 CMake 缓存中的变量
cd build
cmake -LH .. | Select-String "BUILD_.*_TESTS"
```

输出示例：
```
BUILD_LOG_TESTS:BOOL=ON
BUILD_NET_TESTS:BOOL=ON
```

## 🚀 未来扩展

当添加新模块时，遵循相同的模式：

```cmake
# modules/xxx/CMakeLists.txt

# 测试开关
option(BUILD_XXX_TESTS "Build xxx module tests" ON)

# ... 其他配置 ...

# 如果启用测试，添加测试子目录
if(BUILD_XXX_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
```

这样可以保持所有模块的一致性。
