# CMake 依赖传递优化说明

## 问题描述

之前主 CMakeLists.txt 中查找的依赖包无法传递给 modules 子模块，导致每个子模块都需要独立调用 `find_package`，造成以下问题：

1. **重复查找**：每个子模块都独立查找相同的依赖包
2. **配置不一致**：不同模块可能找到不同版本的库
3. **构建效率低**：多次查找相同的包，增加配置时间
4. **维护困难**：依赖版本升级需要在多处修改

## 解决方案

### 1. 调整 CMake 执行顺序

在主 CMakeLists.txt 中，将 `find_package` 调用移到 `add_subdirectory` 之前：

```cmake
# 1. 设置 toolchain（必须在 find_package 之前）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "Vcpkg toolchain file")
endif()

# 2. 查找所有依赖（在 add_subdirectory 之前）
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS ...)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# ... 其他依赖

# 3. 添加子模块（子模块可以访问父作用域的导入目标）
add_subdirectory(modules/log)
add_subdirectory(modules/net)
# ... 其他模块
```

### 2. 移除子模块中的重复 find_package

在每个子模块的 CMakeLists.txt 中，移除 `find_package` 调用，直接使用父作用域中已查找的导入目标：

**修改前：**
```cmake
# modules/log/CMakeLists.txt
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

target_link_libraries(log_lib
    PUBLIC
        spdlog::spdlog
        fmt::fmt
)
```

**修改后：**
```cmake
# modules/log/CMakeLists.txt
# 直接使用父作用域中已查找的包

target_link_libraries(log_lib
    PUBLIC
        spdlog::spdlog
        fmt::fmt
)
```

### 3. CMake 作用域机制

CMake 的作用域规则：
- **父作用域的变量和导入目标对子作用域可见**
- `find_package` 创建的导入目标（如 `spdlog::spdlog`）会自动传递到子目录
- 子模块可以直接使用这些导入目标，无需重新查找

## 修改的文件列表

### 主 CMakeLists.txt
- ✅ 将 `find_package` 移到 `add_subdirectory` 之前
- ✅ 添加缺失的依赖查找：`fmt`、`nlohmann_json`、`sqlite3`
- ✅ 移除重复的 SQLite3 路径设置代码

### 子模块 CMakeLists.txt
- ✅ `modules/log/CMakeLists.txt` - 移除 `find_package(spdlog)` 和 `find_package(fmt)`
- ✅ `modules/net/CMakeLists.txt` - 移除 `find_package(Boost)` 和 `find_package(spdlog)`
- ✅ `modules/config/CMakeLists.txt` - 移除 `find_package(yaml-cpp)`
- ✅ `modules/sqlite/CMakeLists.txt` - 移除 `find_package(sqlite3)`
- ✅ `modules/zlmediakit/CMakeLists.txt` - 移除 `find_package(nlohmann_json)` 和 `find_package(Boost)`
- ✅ `modules/api/CMakeLists.txt` - 移除 `find_package(nlohmann_json)` 和 `find_package(yaml-cpp)`
- ✅ `modules/service/CMakeLists.txt` - 移除 `find_package(yaml-cpp)`

## 优势

1. **单一职责**：依赖查找集中在主 CMakeLists.txt，便于管理
2. **版本一致**：所有模块使用相同版本的依赖库
3. **构建更快**：避免重复查找相同的包
4. **易于维护**：升级依赖版本只需修改一处
5. **符合最佳实践**：遵循 CMake 官方推荐的依赖管理模式

## 注意事项

1. **toolchain 文件必须提前设置**：`CMAKE_TOOLCHAIN_FILE` 必须在任何 `find_package` 之前设置
2. **导入目标自动传递**：Modern CMake 的导入目标（IMPORTED targets）会自动在作用域间传递
3. **保持链接顺序**：子模块之间的依赖关系仍需在 `target_link_libraries` 中明确指定

## 验证方法

重新配置项目后，检查 CMake 输出：
```bash
cd build
cmake ..
```

应该看到：
- ✅ 每个依赖包只被查找一次
- ✅ 没有 "Could NOT find XXX" 错误
- ✅ 所有模块成功配置

## 后续建议

如果未来需要单独构建某个模块（不通过主项目），可以：
1. 在该模块的 CMakeLists.txt 中添加条件判断
2. 仅在 standalone 模式下才调用 `find_package`

示例：
```cmake
# 仅在独立构建时才查找依赖
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 这是顶层项目，需要自己查找依赖
    find_package(spdlog CONFIG REQUIRED)
else()
    # 这是作为子模块被添加，使用父作用域的依赖
endif()
```
