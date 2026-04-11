# API 和 Config 模块重构完成

## 📁 目录结构

### API 模块
```
modules/api/
├── CMakeLists.txt              # 模块级 CMake 配置
├── include/
│   └── api/                    # 公共头文件
│       ├── api_router_registrar.h
│       ├── stream_api_handler.h
│       └── system_api_handler.h
├── src/                        # 源文件
│   ├── api_router_registrar.cpp
│   ├── stream_api_handler.cpp
│   └── system_api_handler.cpp
├── lib/                        # 编译输出的库文件
│   └── api_lib.lib            # (编译后生成)
└── test/                       # 测试文件
    ├── CMakeLists.txt
    └── bin/
```

### Config 模块
```
modules/config/
├── CMakeLists.txt              # 模块级 CMake 配置
├── include/
│   └── config/                 # 公共头文件
│       └── common_config.h
├── src/                        # 源文件
│   └── config_manager.cpp
├── lib/                        # 编译输出的库文件
│   └── config_lib.lib         # (编译后生成)
└── test/                       # 测试文件
    ├── CMakeLists.txt
    ├── bin/
    └── config_test.cpp
```

## 🔧 修改的文件

### 1. modules/api/CMakeLists.txt
- 创建独立的静态库 `api_lib`
- 依赖：nlohmann_json, yaml-cpp, log_lib, net_lib, zlmediakit_lib
- 测试开关默认为 `OFF`

### 2. modules/config/CMakeLists.txt
- 创建独立的静态库 `config_lib`
- 依赖：yaml-cpp, log_lib
- 测试开关默认为 `OFF`

### 3. 根目录 CMakeLists.txt
- 添加 `add_subdirectory(modules/config)`
- 添加 `add_subdirectory(modules/api)`
- 在主程序中链接 `config_lib` 和 `api_lib`

## ✅ 优势

### 1. 清晰的依赖管理
```cmake
# API 模块的依赖
target_link_libraries(api_lib
    PUBLIC
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
        log_lib
        net_lib
        zlmediakit_lib
)

# Config 模块的依赖
target_link_libraries(config_lib
    PUBLIC
        yaml-cpp::yaml-cpp
        log_lib
)
```

### 2. 编译速度提升
- **增量编译**：只修改 api 或 config 模块时，其他模块不需要重新编译
- **并行编译**：各模块可以并行编译
- **缓存友好**：未修改的 .obj 文件可以复用

## 🚀 使用方法

### 编译
```powershell
cd build
cmake ..
cmake --build . --config Debug
```

编译后生成的文件：
- **API 库文件**: `modules/api/lib/api_lib.lib`
- **Config 库文件**: `modules/config/lib/config_lib.lib`

### 启用测试
```powershell
# 启用 API 测试
cmake .. -DBUILD_API_TESTS=ON

# 启用 Config 测试
cmake .. -DBUILD_CONFIG_TESTS=ON

cmake --build . --config Debug
```

### 在其他模块中使用
```cpp
// API 模块
#include "api/api_router_registrar.h"
#include "api/stream_api_handler.h"

// Config 模块
#include "config/common_config.h"

// CMakeLists.txt 中链接
target_link_libraries(your_module PRIVATE api_lib config_lib)
```

## 📊 当前已迁移的模块

```
modules/
├── log/          ✅ 已完成 (BUILD_LOG_TESTS=OFF)
│   └── lib/log_lib.lib
├── net/          ✅ 已完成 (BUILD_NET_TESTS=OFF)
│   └── lib/net_lib.lib
├── sqlite/       ✅ 已完成 (BUILD_SQLITE_TESTS=ON)
│   └── lib/sqlite_lib.lib
├── zlmediakit/   ✅ 已完成 (BUILD_ZLM_TESTS=ON)
│   └── lib/zlmediakit_lib.lib
├── config/       ✅ 已完成 (BUILD_CONFIG_TESTS=OFF)
│   └── lib/config_lib.lib
└── api/          ✅ 已完成 (BUILD_API_TESTS=OFF)
    └── lib/api_lib.lib
```

## 🔍 依赖关系图

```
modules/log/          ← 基础模块
    └── log_lib

modules/net/          ← 依赖 log
    └── net_lib → log_lib

modules/sqlite/       ← 依赖 log
    └── sqlite_lib → log_lib

modules/zlmediakit/   ← 依赖 log + net
    └── zlmediakit_lib → log_lib + net_lib

modules/config/       ← 依赖 log
    └── config_lib → log_lib

modules/api/          ← 依赖 log + net + zlm + config
    └── api_lib → log_lib + net_lib + zlmediakit_lib + config_lib
```

## 💡 注意事项

1. **头文件路径保持不变**：
   - `#include "api/api_router_registrar.h"`
   - `#include "config/common_config.h"`

2. **向后兼容**：旧的 include/ 和 src/ 目录暂时保留作为备份

3. **测试默认禁用**：需要时手动启用

4. **依赖顺序重要**：在主 CMake 中，被依赖的模块要先添加

## 📝 下一步

可以继续迁移其他模块：
1. ✅ log (已完成)
2. ✅ net (已完成)
3. ✅ sqlite (已完成)
4. ✅ zlmediakit (已完成)
5. ✅ config (已完成)
6. ✅ api (已完成)
7. ⏳ puller
8. ⏳ decoder
9. ⏳ preprocess
10. ⏳ postprocess
11. ⏳ alg (包含 gRPC)
12. ⏳ videopipeline
