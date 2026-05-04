# 编译错误修复 - 第二次

## 问题描述

修复第一次编译错误后，出现新的编译错误：

```
error C3861: "LOG_MAIN_INFO_AT": 找不到标识符
error C2440: "初始化": 无法从"std::shared_ptr<ov::Model>"转换为"ov::Model"
error C2665: "ov::Core::compile_model": 没有重载函数可以转换所有参数类型
```

## 根本原因

1. **日志宏未定义**：使用了 `LOG_MAIN_INFO_AT` 但没有包含正确的头文件
2. **OpenVINO API 变化**：`read_model()` 返回 `std::shared_ptr<ov::Model>` 而不是 `ov::Model`
3. **缺少 common_lib 依赖**：inference 模块需要链接 common_lib 以访问日志宏

## 修复方案

### 1. 修复日志头文件包含

**文件**: `modules/alg/inference/src/openvino_cpu_engine.cpp`

```cpp
// 修改前
#include <spdlog/spdlog.h>

// 修改后
#include "common/log/logmanager.h"
```

**说明**: 
- 项目使用自定义的日志宏 `LOG_MAIN_INFO_AT`
- 这些宏定义在 `common/log/logmanager.h` 中
- 需要使用项目的日志系统而不是直接使用 spdlog

---

### 2. 修复 OpenVINO API 调用

**文件**: `modules/alg/inference/src/openvino_cpu_engine.cpp`

```cpp
// 修改前
ov::Model model = core_.read_model(config.model_path);
compiled_model_ = core_.compile_model(model, device, properties);

// 修改后
auto model_ptr = core_.read_model(config.model_path);  // 返回 shared_ptr
compiled_model_ = core_.compile_model(model_ptr, device, properties);  // 接受 shared_ptr
```

**说明**:
- OpenVINO 的 `read_model()` 返回 `std::shared_ptr<ov::Model>`
- `compile_model()` 接受 `std::shared_ptr<const ov::Model>`
- 使用 `auto` 让编译器自动推导类型

---

### 3. 添加 common_lib 依赖

**文件**: `modules/alg/inference/CMakeLists.txt`

```cmake
# 修改前
target_link_libraries(alg_inference
    PUBLIC
        openvino::runtime
        spdlog::spdlog
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)

# 修改后
target_link_libraries(alg_inference
    PUBLIC
        openvino::runtime
        spdlog::spdlog
        common_lib  # ✅ 新增：用于日志宏
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)
```

**说明**:
- `common_lib` 提供了日志宏的定义
- 通过链接 `common_lib`，inference 模块可以访问 `LOG_MAIN_INFO_AT` 等宏
- `common_lib` 的包含目录会自动传递给 inference 模块

---

## 验证步骤

### 1. 清理并重新配置

```bash
cd d:\file_mx\aaaaa\learncpp\out\build\x64-Debug
cmake --build . --target clean
cmake ..
```

或在 Visual Studio 中：
- 项目 → CMake → 删除缓存并重新配置

### 2. 编译 Inference 模块

```bash
cmake --build . --target alg_inference
```

### 3. 预期输出

```
[XX/XX] Building CXX object modules\alg\inference\CMakeFiles\alg_inference.dir\src\openvino_cpu_engine.cpp.obj
[XX/XX] Building CXX object modules\alg\inference\CMakeFiles\alg_inference.dir\src\inference_engine_factory.cpp.obj
[XX/XX] Linking CXX static library D:\file_mx\aaaaa\learncpp\modules\alg\inference\lib\alg_inference.lib
```

---

## 技术细节

### OpenVINO API 说明

#### read_model() 返回值

```cpp
// OpenVINO 2022+ API
std::shared_ptr<ov::Model> Core::read_model(
    const std::string& model_path,
    const std::string& weights_path = ""
);
```

**为什么返回 shared_ptr？**
- 模型对象可能很大，共享所有权避免拷贝
- 多个推理请求可以共享同一个模型
- 符合现代 C++ 最佳实践

#### compile_model() 参数

```cpp
CompiledModel Core::compile_model(
    const std::shared_ptr<const Model>& model,  // 接受 shared_ptr
    const std::string& device_name,
    const AnyMap& properties = {}
);
```

**注意**:
- 第一个参数是 `std::shared_ptr<const Model>`
- 可以直接传递 `read_model()` 的返回值
- 不需要手动解引用

---

### 日志宏系统

项目使用分层日志系统：

```
common/log/logmanager.h
    │
    ├─ LOG_MAIN_INFO_AT(...)      ← 主日志器，带文件/行号
    ├─ LOG_MAIN_ERROR_AT(...)     ← 错误日志
    ├─ LOG_MAIN_WARN_AT(...)      ← 警告日志
    └─ LOG_MAIN_DEBUG_AT(...)     ← 调试日志
    
底层实现:
    └─ spdlog (第三方库)
```

**使用示例**:

```cpp
#include "common/log/logmanager.h"

LOG_MAIN_INFO_AT("Loading model from: {}", path);
LOG_MAIN_ERROR_AT("Failed to load: {}", error_msg);
```

**优势**:
- 自动包含文件名和行号
- 统一的日志格式
- 支持运行时配置
- 线程安全

---

## 常见问题

### Q1: 仍然找不到 logmanager.h

**解决方案**:
1. 确认 common 模块已正确编译
2. 检查 CMake 输出中的包含路径
3. 手动验证文件存在：
   ```
   d:\file_mx\aaaaa\learncpp\modules\common\include\common\log\logmanager.h
   ```

### Q2: OpenVINO 版本不兼容

**检查版本**:
```bash
# 查看安装的 OpenVINO 版本
vcpkg list | findstr openvino
```

**推荐版本**: 2023.x 或更高

**如果版本太旧**:
```bash
vcpkg update
vcpkg upgrade openvino
```

### Q3: 链接错误 "unresolved external symbol"

**可能原因**:
- common_lib 未正确链接
- 编译顺序问题

**解决方案**:
1. 清理并重新构建整个项目
2. 检查 CMake 输出确认 common_lib 已编译
3. 验证链接命令中包含 common_lib

---

## 相关文件清单

本次修复涉及的文件：

1. ✅ `modules/alg/inference/src/openvino_cpu_engine.cpp`
   - 修改日志头文件
   - 修复 OpenVINO API 调用

2. ✅ `modules/alg/inference/CMakeLists.txt`
   - 添加 common_lib 依赖

---

## 下一步

编译成功后，可以：

1. **运行测试**:
   ```bash
   cd bin
   ./test_inference.exe
   ```

2. **运行示例**:
   ```bash
   ./inference_example.exe
   ```

3. **继续开发**:
   - Preprocessor 模块
   - Postprocessor 模块
   - Algorithm 封装

---

**修复日期**: 2026-05-03  
**修复人**: Lingma AI Assistant  
**状态**: ✅ 已修复
