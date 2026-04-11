# ZLMediaKit 模块重构完成

## 📁 目录结构

```
modules/zlmediakit/
├── CMakeLists.txt              # 模块级 CMake 配置
├── include/
│   └── zlmediakit/             # 公共头文件
│       ├── zlm_hookserver.h
│       ├── zlm_httpclient.h
│       ├── zlm_manager.h
│       ├── zlm_proxy_manager.h
│       ├── zlm_record_manager.h
│       ├── zlm_rtp_manager.h
│       ├── zlm_stream_manager.h
│       └── zlm_system_manager.h
├── src/                        # 源文件
│   ├── zlm_hookserver.cpp
│   ├── zlm_httpclient.cpp
│   ├── zlm_manager.cpp
│   ├── zlm_proxy_manager.cpp
│   ├── zlm_record_manager.cpp (如果有)
│   ├── zlm_rtp_manager.cpp (如果有)
│   ├── zlm_stream_manager.cpp (如果有)
│   └── zlm_system_manager.cpp (如果有)
├── lib/                        # 编译输出的库文件
│   └── zlmediakit_lib.lib     # (编译后生成)
└── test/                       # 测试文件
    ├── CMakeLists.txt          # 测试 CMake 配置
    ├── bin/                    # 测试可执行文件输出目录
    └── zlm.cpp
```

## 🔧 修改的文件

### 1. modules/zlmediakit/CMakeLists.txt
- 创建独立的静态库 `zlmediakit_lib`
- 自动查找并链接 nlohmann_json
- 依赖 log_lib 和 net_lib
- 测试开关默认为 `OFF`
- 支持可选的测试构建

### 2. modules/zlmediakit/test/CMakeLists.txt
- 自动扫描所有测试文件
- 为每个测试创建独立的可执行文件
- 链接到 `zlmediakit_lib`

### 3. 根目录 CMakeLists.txt
- 添加 `add_subdirectory(modules/zlmediakit)`
- 在主程序中链接 `zlmediakit_lib`

### 4. 主程序链接更新
```cmake
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        log_lib
        net_lib
        sqlite_lib
        zlmediakit_lib  # ← 新增
        ...
)
```

## ✅ 优势

### 1. 清晰的依赖管理
```cmake
# zlmediakit 模块明确声明了依赖
target_link_libraries(zlmediakit_lib
    PUBLIC
        nlohmann_json::nlohmann_json
        log_lib
        net_lib
)
```

### 2. 编译速度提升
- **增量编译**：只修改 zlmediakit 模块时，其他模块不需要重新编译
- **并行编译**：zlmediakit_lib 可以与其他模块并行编译
- **缓存友好**：未修改的 .obj 文件可以复用

### 3. 可复用性
- 其他项目可以直接使用 `zlmediakit_lib`
- 可以轻松发布为独立的库

## 🚀 使用方法

### 编译
```powershell
cd build
cmake ..
cmake --build . --config Debug
```

编译后生成的文件：
- **库文件**: `modules/zlmediakit/lib/zlmediakit_lib.lib`
- **测试可执行文件** (如果启用): `modules/zlmediakit/test/bin/test_zlm_*.exe`

### 启用测试
```powershell
# 默认测试是禁用的，需要显式启用
cmake .. -DBUILD_ZLM_TESTS=ON
cmake --build . --config Debug

# 运行测试
.\modules\zlmediakit\test\bin\test_zlm.exe
```

### 在其他模块中使用
```cpp
// 包含头文件
#include "zlmediakit/zlm_manager.h"
#include "zlmediakit/zlm_httpclient.h"

// CMakeLists.txt 中链接
target_link_libraries(your_module PRIVATE zlmediakit_lib)
```

## 📝 下一步

可以继续迁移其他模块：
1. ✅ log (已完成)
2. ✅ net (已完成)
3. ✅ sqlite (已完成)
4. ✅ zlmediakit (已完成)
5. ⏳ puller
6. ⏳ decoder
7. ⏳ preprocess
8. ⏳ postprocess
9. ⏳ alg (包含 gRPC)
10. ⏳ videopipeline

## 💡 注意事项

1. **头文件路径保持不变**：仍然是 `#include "zlmediakit/zlm_manager.h"`
2. **向后兼容**：旧的 include/zlmediakit/ 和 src/zlmediakit/ 目录暂时保留作为备份
3. **测试默认禁用**：`BUILD_ZLM_TESTS` 默认为 `OFF`，需要时手动启用
4. **依赖关系**：zlmediakit 依赖 log 和 net 模块

## 📊 当前已迁移的模块

```
modules/
├── log/          ✅ 已完成 (BUILD_LOG_TESTS=OFF)
│   └── lib/log_lib.lib
├── net/          ✅ 已完成 (BUILD_NET_TESTS=OFF)
│   └── lib/net_lib.lib
├── sqlite/       ✅ 已完成 (BUILD_SQLITE_TESTS=ON)
│   └── lib/sqlite_lib.lib
└── zlmediakit/   ✅ 已完成 (BUILD_ZLM_TESTS=OFF)
    └── lib/zlmediakit_lib.lib
```

## 🔍 依赖关系

```
modules/log/          ← 基础模块，无内部依赖
    └── log_lib

modules/net/          ← 依赖 log 模块
    └── net_lib → log_lib

modules/sqlite/       ← 依赖 log 模块
    └── sqlite_lib → log_lib

modules/zlmediakit/   ← 依赖 log 和 net 模块
    └── zlmediakit_lib → log_lib + net_lib
```
