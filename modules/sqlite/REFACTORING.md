# SQLite 模块重构完成

## 📁 目录结构

```
modules/sqlite/
├── CMakeLists.txt              # 模块级 CMake 配置
├── include/
│   └── sqlite/                 # 公共头文件
│       └── sqlite.h
├── src/                        # 源文件
│   └── sqlite.cpp
├── lib/                        # 编译输出的库文件
│   └── sqlite_lib.lib         # (编译后生成)
└── test/                       # 测试文件
    ├── CMakeLists.txt          # 测试 CMake 配置
    ├── bin/                    # 测试可执行文件输出目录
    ├── test_multi_instance.cpp
    ├── test_sqlite.cpp
    ├── test_sqlite_example.cpp
    └── usage_comparison.cpp
```

## 🔧 修改的文件

### 1. modules/sqlite/CMakeLists.txt
- 创建独立的静态库 `sqlite_lib`
- 自动查找并链接 SQLite3 库（支持 Windows/Linux）
- 测试开关默认为 `OFF`（因为 SQLite 测试较多）
- 支持可选的测试构建

### 2. modules/sqlite/test/CMakeLists.txt
- 自动扫描所有测试文件
- 为每个测试创建独立的可执行文件
- 链接到 `sqlite_lib`

### 3. 根目录 CMakeLists.txt
- 添加 `add_subdirectory(modules/sqlite)`
- 在主程序中链接 `sqlite_lib`
- 移除旧的直接链接 `${SQLITE3_LIB_PATH}` 方式

### 4. 主程序链接更新
```cmake
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        log_lib
        net_lib
        sqlite_lib  # ← 新增：使用模块化方式
        ...
        # "${SQLITE3_LIB_PATH}"  # ← 已移除：不再直接链接
)
```

## ✅ 优势

### 1. 统一的依赖管理
```cmake
# 旧方式：每个地方都要手动配置 SQLite3 路径
"${SQLITE3_LIB_PATH}"

# 新方式：通过模块统一管理
sqlite_lib  # 内部处理了所有平台差异
```

### 2. 跨平台支持
```cmake
# Windows: 自动从 vcpkg 查找
set(SQLITE3_LIB_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib/sqlite3.lib")

# Linux: 使用 find_package
find_package(SQLite3 REQUIRED)
```

### 3. 编译速度提升
- **增量编译**：只修改 sqlite 模块时，其他模块不需要重新编译
- **并行编译**：sqlite_lib 可以与其他模块并行编译
- **缓存友好**：未修改的 .obj 文件可以复用

### 4. 可复用性
- 其他项目可以直接使用 `sqlite_lib`
- 可以轻松发布为独立的库

## 🚀 使用方法

### 编译
```powershell
cd build
cmake ..
cmake --build . --config Debug
```

编译后生成的文件：
- **库文件**: `modules/sqlite/lib/sqlite_lib.lib`
- **测试可执行文件** (如果启用): `modules/sqlite/test/bin/test_sqlite_*.exe`

### 启用测试
```powershell
# 默认测试是禁用的，需要显式启用
cmake .. -DBUILD_SQLITE_TESTS=ON
cmake --build . --config Debug

# 运行测试
.\modules\sqlite\test\bin\test_sqlite.exe
```

### 在其他模块中使用
```cpp
// 包含头文件
#include "sqlite/sqlite.h"

// CMakeLists.txt 中链接
target_link_libraries(your_module PRIVATE sqlite_lib)
```

## 📝 下一步

可以继续迁移其他模块：
1. ✅ log (已完成)
2. ✅ net (已完成)
3. ✅ sqlite (已完成)
4. ⏳ puller
5. ⏳ decoder
6. ⏳ preprocess
7. ⏳ postprocess
8. ⏳ alg (包含 gRPC)
9. ⏳ videopipeline

## 💡 注意事项

1. **头文件路径保持不变**：仍然是 `#include "sqlite/sqlite.h"`
2. **向后兼容**：旧的 include/sqlite/ 和 src/sqlite/ 目录暂时保留作为备份
3. **测试默认禁用**：`BUILD_SQLITE_TESTS` 默认为 `OFF`，需要时手动启用
4. **跨平台支持**：自动处理 Windows 和 Linux 的 SQLite3 库查找

## 📊 当前已迁移的模块

```
modules/
├── log/          ✅ 已完成
│   ├── lib/log_lib.lib
│   └── test/ (可选)
├── net/          ✅ 已完成
│   ├── lib/net_lib.lib
│   └── test/ (可选)
└── sqlite/       ✅ 已完成
    ├── lib/sqlite_lib.lib
    └── test/ (可选，默认禁用)
```

## 🔍 依赖关系

```
modules/log/          ← 基础模块，无内部依赖
    └── log_lib

modules/net/          ← 依赖 log 模块
    └── net_lib → log_lib

modules/sqlite/       ← 基础模块，无内部依赖
    └── sqlite_lib
```
