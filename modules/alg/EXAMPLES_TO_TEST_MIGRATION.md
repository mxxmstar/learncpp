# Examples 移动到 Test 目录

## 📋 变更说明

将 `examples/` 目录中的示例程序移动到 `test/` 目录，统一管理所有测试和示例代码。

---

## 🔄 执行的操作

### 1. 移动文件

```powershell
# 移动 examples 中的所有文件到 test
Move-Item -Path examples/* -Destination test/ -Force

# 删除空的 examples 目录
Remove-Item -Path examples -Force
```

### 2. 更新 CMakeLists.txt

**文件**: `modules/alg/test/CMakeLists.txt`

添加了两个新的可执行目标：

#### test_inference（单元测试）

```cmake
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_inference.cpp")
    add_executable(test_inference
        test_inference.cpp
    )

    target_link_libraries(test_inference
        PRIVATE
            alg_lib
            common_lib
            openvino::runtime
            ${CMAKE_THREAD_LIBS_INIT}
    )

    target_compile_features(test_inference PRIVATE cxx_std_20)
endif()
```

#### inference_example（示例程序）

```cmake
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/inference_example.cpp")
    add_executable(inference_example
        inference_example.cpp
    )

    target_link_libraries(inference_example
        PRIVATE
            alg_lib
            common_lib
            openvino::runtime
            ${CMAKE_THREAD_LIBS_INIT}
    )

    target_compile_features(inference_example PRIVATE cxx_std_20)
endif()
```

### 3. 注册测试

```cmake
if(TARGET test_inference)
    add_test(NAME test_inference
        COMMAND test_inference
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()
```

---

## 📂 最终结构

### 之前

```
modules/alg/
├── examples/
│   └── inference_example.cpp
└── test/
    ├── test_inference.cpp
    ├── test_grpc_*.cpp
    └── CMakeLists.txt
```

### 现在 ✅

```
modules/alg/
└── test/
    ├── inference_example.cpp      # ✅ 从 examples 移动过来
    ├── test_inference.cpp         # 单元测试
    ├── test_grpc_video_client.cpp
    ├── test_grpc_algorithm_integration.cpp
    ├── test_video_grpc_client.cpp
    ├── test_video_processor.cpp
    ├── *.py                       # Python 测试脚本
    ├── bin/                       # 编译输出
    └── CMakeLists.txt             # ✅ 已更新
```

---

## 🎯 优势

### 1. **统一管理**
- 所有测试和示例都在 `test/` 目录
- 便于查找和维护
- 统一的构建配置

### 2. **简化结构**
- 减少目录层级
- 更清晰的项目结构
- 避免 examples 和 test 的重复

### 3. **一致的构建流程**
- 所有测试程序使用相同的 CMake 配置
- 统一的依赖管理
- 更容易添加新测试

---

## 🔧 编译和运行

### 编译所有测试

```bash
cd d:\file_mx\aaaaa\learncpp\out\build\x64-Debug
cmake ..
cmake --build . --target alg_lib
cmake --build . --target test_inference
cmake --build . --target inference_example
```

### 运行测试

```bash
# 进入测试输出目录
cd modules/alg/test/bin

# 运行单元测试
./test_inference.exe

# 运行示例程序
./inference_example.exe
```

### CTest 集成

```bash
# 运行所有注册的测试
ctest -R test_inference

# 运行特定测试
ctest -R inference
```

---

## 📊 文件清单

### 移动的文件

| 文件 | 原位置 | 新位置 | 类型 |
|------|--------|--------|------|
| `inference_example.cpp` | `examples/` | `test/` | 示例程序 |

### 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `test/CMakeLists.txt` | ① 添加 test_inference 目标 ② 添加 inference_example 目标 ③ 注册测试 ④ 更新消息输出 |

---

## 💡 使用建议

### 区分测试和示例

虽然都在 `test/` 目录，但有两种不同类型的程序：

#### 单元测试 (test_*.cpp)
- **目的**: 验证功能正确性
- **特点**: 
  - 自动化测试
  - 有断言和检查
  - 可以通过 CTest 运行
- **命名**: `test_<module>.cpp`

#### 示例程序 (*_example.cpp)
- **目的**: 展示如何使用 API
- **特点**:
  - 交互式演示
  - 详细的注释
  - 需要手动运行
- **命名**: `<module>_example.cpp`

### 添加新测试

1. **单元测试**:
   ```cpp
   // test/my_feature.cpp
   #include <cassert>
   
   void test_my_feature() {
       // 测试逻辑
       assert(result == expected);
   }
   
   int main() {
       test_my_feature();
       return 0;
   }
   ```

2. **示例程序**:
   ```cpp
   // my_feature_example.cpp
   #include "alg/inference/..."
   
   int main() {
       // 展示如何使用
       auto engine = CreateEngine();
       engine->Process(...);
       return 0;
   }
   ```

3. **更新 CMakeLists.txt**:
   ```cmake
   if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/my_feature_example.cpp")
       add_executable(my_feature_example
           my_feature_example.cpp
       )
       target_link_libraries(my_feature_example PRIVATE alg_lib ...)
       target_compile_features(my_feature_example PRIVATE cxx_std_20)
   endif()
   ```

---

## ⚠️ 注意事项

### 1. 输出目录

所有测试程序都输出到 `test/bin/` 目录：

```cmake
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
```

### 2. 依赖项

确保链接了所有必要的库：
- `alg_lib` - 算法库
- `common_lib` - 通用库（日志等）
- `openvino::runtime` - OpenVINO
- `${CMAKE_THREAD_LIBS_INIT}` - 线程库

### 3. C++ 标准

所有测试都需要 C++20：

```cmake
target_compile_features(<target> PRIVATE cxx_std_20)
```

---

## 🚀 下一步

1. ✅ **完成移动** - 已完成
2. ⏳ **重新编译** - 需要验证
3. ⏳ **运行测试** - 确认功能正常
4. ⏳ **更新文档** - 如有必要

---

**变更日期**: 2026-05-04  
**变更人**: Lingma AI Assistant  
**状态**: ✅ 已完成
