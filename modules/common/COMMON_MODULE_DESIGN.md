# Common 模块设计说明

## 🎯 当前状态

### 模块内容

**头文件**:
- ✅ `include/common/iservice.h` - 服务接口定义（纯抽象）

**源文件**:
- ✅ `src/common.cpp` - 占位文件（确保库可以编译）

**特点**:
- ✅ 只有接口定义，没有实现
- ✅ 无外部依赖
- ✅ 可以被任何模块依赖

---

## 📊 为什么需要占位源文件？

### 问题

如果 CMakeLists.txt 中没有源文件：

```cmake
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")
# LIB_SOURCES 为空！

add_library(${PROJECT_NAME} ${LIB_SOURCES})
# ❌ CMake 错误：无法创建空库
```

---

### 解决方案

#### 方案 1: 接口库（INTERFACE library）

```cmake
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
```

**优点**:
- ✅ 不需要源文件
- ✅ 纯粹的头文件库

**缺点**:
- ❌ 不能添加编译定义
- ❌ 不能链接其他库
- ❌ 将来添加实现时需要重构

---

#### 方案 2: 占位源文件（✅ 推荐）

```cmake
# src/common.cpp - 占位文件
add_library(${PROJECT_NAME} src/common.cpp)
```

**优点**:
- ✅ 可以正常编译
- ✅ 可以添加编译定义
- ✅ 可以链接其他库
- ✅ **便于将来扩展**

**缺点**:
- ⚠️ 有一个几乎空的源文件

---

### 选择方案 2 的原因

你提到："**后续可能需要添加一些通用的功能，需要往后兼容**"

**方案 2 的优势**:
1. ✅ **向后兼容** - 未来可以直接添加 `.cpp` 文件
2. ✅ **灵活性** - 可以链接其他库（如需要通用工具依赖第三方库）
3. ✅ **一致性** - 与其他模块的 CMake 配置一致
4. ✅ **无需重构** - 添加实现时不需要修改 CMakeLists.txt

---

## 🔮 未来扩展方向

### 可以添加到 common 模块的内容

#### 1. 通用工具函数

```cpp
// include/common/string_utils.h
#pragma once
#include <string>

namespace common {
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    bool startsWith(const std::string& str, const std::string& prefix);
}

// src/string_utils.cpp
#include "common/string_utils.h"

namespace common {
    std::string trim(const std::string& str) {
        // 实现...
    }
}
```

---

#### 2. 文件操作工具

```cpp
// include/common/file_utils.h
#pragma once
#include <string>
#include <vector>

namespace common {
    bool fileExists(const std::string& path);
    std::string readFile(const std::string& path);
    bool writeFile(const std::string& path, const std::string& content);
    std::vector<std::string> listFiles(const std::string& directory);
}
```

---

#### 3. 时间日期工具

```cpp
// include/common/time_utils.h
#pragma once
#include <string>
#include <chrono>

namespace common {
    std::string getCurrentTimestamp();
    std::string formatTime(std::chrono::system_clock::time_point time);
    uint64_t currentTimeMillis();
}
```

---

#### 4. 数学工具

```cpp
// include/common/math_utils.h
#pragma once

namespace common {
    constexpr double PI = 3.14159265358979323846;
    
    inline double deg2rad(double deg) {
        return deg * PI / 180.0;
    }
    
    inline double rad2deg(double rad) {
        return rad * 180.0 / PI;
    }
    
    template<typename T>
    T clamp(T value, T min, T max) {
        return (value < min) ? min : (value > max) ? max : value;
    }
}
```

---

#### 5. 日志工具封装

```cpp
// include/common/logger.h
#pragma once
#include <string>

namespace common {
    enum class LogLevel {
        DEBUG,
        INFO,
        WARN,
        ERROR
    };
    
    void log(LogLevel level, const std::string& message);
    void logDebug(const std::string& message);
    void logInfo(const std::string& message);
    void logWarn(const std::string& message);
    void logError(const std::string& message);
}
```

---

### 添加新功能的步骤

#### Step 1: 创建头文件

```cpp
// include/common/xxx_utils.h
#pragma once

namespace common {
    // 声明函数
}
```

---

#### Step 2: 创建实现文件

```cpp
// src/xxx_utils.cpp
#include "common/xxx_utils.h"

namespace common {
    // 实现函数
}
```

---

#### Step 3: 更新 CMakeLists.txt（如果需要）

```cmake
# 如果添加了新的依赖
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        some_dependency  # 添加依赖
)
```

**注意**: 如果只是添加 `.cpp` 文件，`GLOB_RECURSE` 会自动包含，无需修改 CMakeLists.txt！

---

#### Step 4: 在其他模块中使用

```cpp
// 其他模块的代码
#include "common/xxx_utils.h"

void someFunction() {
    common::someUtilityFunction();
}
```

---

## 📝 CMakeLists.txt 说明

### 当前配置

```cmake
cmake_minimum_required(VERSION 3.18)
project(common_lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 收集源文件（包括占位文件 common.cpp）
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")

# 创建库
add_library(${PROJECT_NAME} ${LIB_SOURCES})

# 设置包含目录
target_include_directories(${PROJECT_NAME} 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接依赖（目前为空，未来可以添加）
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        # 预留：未来可以添加依赖
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
```

---

### 关键配置说明

#### 1. GLOB_RECURSE

```cmake
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")
```

**作用**: 自动收集 `src/` 目录下所有的 `.cpp` 文件

**优势**: 
- ✅ 添加新文件时无需修改 CMakeLists.txt
- ✅ 便于扩展

---

#### 2. PUBLIC 包含目录

```cmake
target_include_directories(${PROJECT_NAME} 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

**作用**: 让依赖 common_lib 的模块可以 `#include "common/xxx.h"`

**PUBLIC vs PRIVATE**:
- `PUBLIC`: 依赖此库的模块也可以使用这些头文件
- `PRIVATE`: 只有此库内部可以使用

---

#### 3. 空的 target_link_libraries

```cmake
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        # 预留：未来可以添加依赖
)
```

**原因**: 
- 目前 common_lib 只有接口，不需要依赖其他库
- 保留结构，便于将来添加依赖
- 如果完全删除这段，将来添加依赖时需要重新编写

---

## 💡 最佳实践

### 1. 保持 common 模块简洁

**应该放入 common**:
- ✅ 纯工具函数（无状态）
- ✅ 基础数据类型
- ✅ 通用接口定义（如 IService）
- ✅ 不依赖业务逻辑的代码

**不应该放入 common**:
- ❌ 业务逻辑
- ❌ 网络代码
- ❌ 数据库代码
- ❌ 依赖特定框架的代码

---

### 2. 使用命名空间

```cpp
namespace common {
    namespace string {
        std::string trim(...);
    }
    
    namespace file {
        bool exists(...);
    }
    
    namespace time {
        std::string now();
    }
}
```

**优势**:
- ✅ 避免命名冲突
- ✅ 清晰的分类
- ✅ 易于查找

---

### 3. 提供单元测试

```
modules/common/
├── include/common/
├── src/
└── test/
    ├── test_string_utils.cpp
    ├── test_file_utils.cpp
    └── test_time_utils.cpp
```

---

### 4. 文档化

为每个工具函数添加注释：

```cpp
namespace common {
    /// @brief 去除字符串两端的空白字符
    /// @param str 输入字符串
    /// @return 去除空白后的字符串
    std::string trim(const std::string& str);
}
```

---

## 🎯 总结

### 当前设计

- ✅ **占位源文件** - 确保库可以编译
- ✅ **GLOB_RECURSE** - 自动收集新文件
- ✅ **空的依赖列表** - 预留扩展空间
- ✅ **向后兼容** - 未来可以直接添加功能

---

### 未来扩展

1. **添加工具函数** - 直接创建 `.h` 和 `.cpp` 文件
2. **添加依赖** - 在 `target_link_libraries` 中添加
3. **无需重构** - CMake 配置已经准备好

---

### 关键原则

- ✅ **简洁** - 只放真正通用的功能
- ✅ **无依赖** - 尽量不依赖外部库
- ✅ **纯函数** - 优先使用无状态的工具函数
- ✅ **文档化** - 清晰的注释和示例

---

## 🔗 相关文档

- [ISERVICE_IN_COMMON.md](ISERVICE_IN_COMMON.md) - IService 为什么在 common
- [APPLICATION_MODULE_SPLIT.md](../application/APPLICATION_MODULE_SPLIT.md) - Application 模块拆分
