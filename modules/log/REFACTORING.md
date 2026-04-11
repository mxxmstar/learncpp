# Log 模块重构完成

## 📁 新的目录结构

```
modules/log/
├── CMakeLists.txt              # 模块级 CMake 配置
├── include/
│   └── log/                    # 公共头文件（带子目录避免冲突）
│       ├── logger.h
│       └── logmanager.h
├── src/                        # 源文件
│   ├── logger.cpp
│   └── logmanager.cpp
├── lib/                        # 编译输出的库文件
│   └── log_lib.lib
├── REFACTORING.md              # 重构说明文档
└── test/                       # 测试文件
    ├── CMakeLists.txt          # 测试 CMake 配置
    ├── bin/                    # 测试可执行文件输出目录
    ├── logger.cpp
    └── logmanage.cpp
```

## 🔧 修改的文件

### 1. modules/log/CMakeLists.txt
- 创建独立的静态库 `log_lib`
- 自动查找并链接 spdlog 和 fmt
- 支持可选的测试构建

### 2. modules/log/test/CMakeLists.txt
- 自动扫描所有测试文件
- 为每个测试创建独立的可执行文件
- 链接到 `log_lib`

### 3. 根目录 CMakeLists.txt
- 添加 `add_subdirectory(modules/log)`
- 在主程序和 myapp_lib 中链接 `log_lib`
- **禁用全局 test 目录**：测试由各模块自己管理

### 4. test/CMakeLists.txt
- 注释掉旧的 log 测试配置
- 测试已迁移到 modules/log/test/

## ✅ 优势

### 编译速度提升
- **增量编译**：只修改 log 模块时，其他模块不需要重新编译
- **并行编译**：log_lib 可以与其他模块并行编译
- **缓存友好**：未修改的 .obj 文件可以复用

### 依赖管理清晰
```cmake
# 其他模块只需要链接 log_lib
target_link_libraries(other_module PRIVATE log_lib)
```

### 可复用性
- 其他项目可以直接使用 log_lib
- 可以轻松发布为独立的库

## 🚀 使用方法

### 编译 log 模块
```powershell
cd build
cmake ..
cmake --build . --config Debug
```

编译后生成的文件：
- **库文件**: `modules/log/lib/log_lib.lib`
- **测试可执行文件**: `modules/log/test/bin/test_log_*.exe`

### 运行测试
```powershell
# 直接运行测试可执行文件
.\modules\log\test\bin\test_log_logger.exe
.\modules\log\test\bin\test_log_logmanage.exe

# 或者使用 ctest（如果在模块内启用了）
cd modules/log/test/bin
ctest -C Debug --verbose
```

### 在其他模块中使用
```cpp
// 包含头文件
#include "log/logger.h"
#include "log/logmanager.h"

// CMakeLists.txt 中链接
target_link_libraries(your_module PRIVATE log_lib)
```

## 📝 下一步

可以继续迁移其他模块：
1. ✅ log (已完成)
2. ⏳ net
3. ⏳ puller
4. ⏳ decoder
5. ⏳ preprocess
6. ⏳ postprocess
7. ⏳ alg
8. ⏳ videopipeline

## 💡 注意事项

1. **头文件路径保持不变**：仍然是 `#include "log/logger.h"`
2. **向后兼容**：旧的 include/log/ 和 src/log/ 目录暂时保留作为备份
3. **测试隔离**：log 模块的测试完全独立，不依赖其他模块
