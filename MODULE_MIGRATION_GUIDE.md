# 模块化重构说明

## 🎯 核心变更

### 1. 主 CMake 不再扫描 src 目录
```cmake
# 旧方式（已废弃）
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})

# 新方式
add_executable(${PROJECT_NAME} apps/main.cpp)
```

### 2. myapp_lib 已废弃
```cmake
# 旧方式（已注释）
# file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")
# add_library(myapp_lib ${LIB_SOURCES})

# 新方式：直接使用各模块的库
target_link_libraries(${PROJECT_NAME} PRIVATE log_lib ...)
```

### 3. 测试由各模块自己管理
```cmake
# 旧方式（已注释）
# add_subdirectory(test)

# 新方式：每个模块有自己的 test 子目录
add_subdirectory(modules/log)  # 内部会处理自己的测试
```

## 📁 新的编译流程

### 编译主程序
```powershell
cd build
cmake ..
cmake --build . --config Debug
```

**只会编译：**
- `apps/main.cpp`
- `modules/log/src/*.cpp` (以及后续添加的其他模块)

**不会编译：**
- `src/net/*.cpp` ❌
- `src/puller/*.cpp` ❌
- `src/decoder/*.cpp` ❌
- 其他未迁移到 modules 的代码 ❌

### 编译和测试 log 模块
```powershell
# 编译
cd build
cmake ..
cmake --build . --config Debug

# 运行测试
.\modules\log\test\bin\test_log_logger.exe
.\modules\log\test\bin\test_log_logmanage.exe
```

## 🔄 迁移策略

### 阶段 1：当前状态
- ✅ log 模块已迁移到 `modules/log/`
- ⏳ 其他模块仍在 `src/` 和 `include/` 目录
- ⚠️ 主程序只能使用 log_lib，其他功能暂时不可用

### 阶段 2：逐步迁移
按以下顺序迁移其他模块：
1. net
2. puller
3. decoder
4. preprocess
5. postprocess
6. alg
7. videopipeline

每迁移一个模块：
```powershell
# 1. 创建 modules/xxx/ 目录结构
# 2. 移动文件
# 3. 编写 CMakeLists.txt
# 4. 在主 CMake 中添加 add_subdirectory(modules/xxx)
# 5. 在主程序中链接 xxx_lib
# 6. 测试
```

### 阶段 3：清理旧代码
当所有模块都迁移完成后：
```powershell
# 删除旧的 src/ 目录（保留 main.cpp）
# 删除旧的 include/ 目录
# 删除 test/ 目录
```

## 💡 优势

### 1. 编译速度
- **修改 log 模块**：只重新编译 log_lib（几秒钟）
- **修改其他模块**：log_lib 不需要重新编译
- **并行编译**：各模块可以并行编译

### 2. 依赖清晰
```cmake
# 一眼就能看出依赖关系
target_link_libraries(videopipeline_lib
    PRIVATE
        log_lib
        puller_lib
        decoder_lib
        preprocess_lib
        alg_lib
)
```

### 3. 独立测试
```powershell
# 测试单个模块
.\modules\log\test\bin\test_log_logger.exe

# 不需要编译整个项目
```

### 4. 可复用性
```cmake
# 其他项目可以直接使用
find_package(log_lib REQUIRED)
target_link_libraries(my_app PRIVATE log_lib)
```

## ⚠️ 注意事项

### 当前限制
1. **只有 log 模块可用**：其他功能尚未迁移
2. **main.cpp 可能编译失败**：如果引用了未迁移的模块
3. **旧代码仍然存在**：`src/` 和 `include/` 目录还在，但不会被编译

### 解决方案
**临时方案**：在 main.cpp 中注释掉未迁移模块的代码

**长期方案**：尽快迁移所有模块

## 📊 对比

| 特性 | 旧方式 | 新方式 |
|------|--------|--------|
| 编译范围 | 所有 src/*.cpp | 只有 main.cpp + modules/* |
| 增量编译 | 差（任何修改都可能触发全量编译） | 好（只编译修改的模块） |
| 测试隔离 | 无（所有测试一起编译） | 有（每个模块独立测试） |
| 依赖管理 | 混乱（myapp_lib 包含一切） | 清晰（显式链接各模块） |
| 可复用性 | 低（耦合在一起） | 高（独立库） |

## 🚀 下一步

1. **测试当前配置**：验证 log 模块是否能正常编译和使用
2. **迁移 net 模块**：按照 log 的模式继续
3. **更新 main.cpp**：确保只使用已迁移的模块
4. **建立模板**：将 modules/log/ 作为其他模块的模板
