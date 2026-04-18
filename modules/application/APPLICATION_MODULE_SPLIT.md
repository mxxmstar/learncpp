# Application 模块拆分说明

## 🎯 拆分原因

### 问题：循环依赖

之前的架构存在循环依赖问题：

```
common_lib (IService, ServiceContainer, Application)
    ↑ ↓
zlmediakit_lib (ZLMService : IService)
    ↑ ↓  
web_lib (使用各种服务)
    ↑ ↓
common_lib (Application 初始化时需要知道所有服务)
```

**根本原因**:
- `common` 模块既包含基础设施（IService），又包含应用框架（Application）
- Application 需要注册各个模块的服务
- 但服务又在各个模块中
- 形成循环依赖！

---

## ✅ 解决方案

将 `common` 模块拆分为两个独立的模块：

### 1. **common 模块** - 纯工具类
只包含无业务逻辑的通用工具：
- 日志工具
- 配置管理
- 字符串处理
- 文件操作
- 等...

**特点**: 
- ✅ 无业务逻辑
- ✅ 可以被任何模块依赖
- ✅ 不会依赖其他业务模块

---

### 2. **application 模块** - 应用框架层
包含应用级别的基础设施：
- `IService` - 服务接口
- `ServiceContainer` - 服务容器
- `Application` - 应用框架
- `SignalHandler` - 信号处理

**特点**:
- ✅ 定义服务规范
- ✅ 管理服务生命周期
- ✅ 被业务模块依赖
- ✅ 不依赖具体业务模块

---

## 📊 新的架构

### 目录结构

```
modules/
├── common/              # 纯工具类（未来可以放通用工具）
│   └── ...
│
├── application/         # ← 新建：应用框架层
│   ├── include/application/
│   │   ├── iservice.h           # 服务接口
│   │   ├── service_container.h  # 服务容器
│   │   ├── application.h        # 应用框架
│   │   └── signal_handler.h     # 信号处理
│   ├── src/
│   │   ├── application.cpp
│   │   └── signal_handler.cpp
│   └── CMakeLists.txt
│
├── zlmediakit/          # 业务模块
│   └── include/zlmediakit/service/
│       └── zlm_service.h  (实现 IService)
│
├── web/                 # 业务模块
│   └── include/web/service/
│       ├── http_server_service.h
│       └── httpclient_pool_service.h
│
└── ...
```

---

### 依赖关系

```
application_lib (IService, ServiceContainer)
    ↑
zlmediakit_lib (ZLMService : IService)
    ↑
web_lib (HttpServerService, HttpClientPoolService : IService)
    ↑
main.cpp (组合所有模块，注册服务)
```

**✅ 清晰的单向依赖，无循环！**

---

## 🔧 迁移内容

### 从 common 移动到 application

#### 头文件
```
modules/common/include/common/
├── service/iservice.h              → modules/application/include/application/iservice.h
├── service/service_container.h     → modules/application/include/application/service_container.h
├── application.h                   → modules/application/include/application/application.h
└── signal_handler.h                → modules/application/include/application/signal_handler.h
```

#### 源文件
```
modules/common/src/
├── application.cpp                 → modules/application/src/application.cpp
└── signal_handler.cpp              → modules/application/src/signal_handler.cpp
```

---

## 📝 更新的文件

### 1. 创建新文件
- ✅ `modules/application/CMakeLists.txt`
- ✅ `modules/application/include/application/` (目录)
- ✅ `modules/application/src/` (目录)

---

### 2. 更新 include 路径（8个文件）

#### zlmediakit 模块
- ✅ `modules/zlmediakit/include/zlmediakit/service/zlm_service.h`
  ```cpp
  // 之前
  #include "common/service/iservice.h"
  
  // 之后
  #include "application/iservice.h"
  ```

---

#### web 模块
- ✅ `modules/web/include/web/service/http_server_service.h`
- ✅ `modules/web/include/web/service/httpclient_pool_service.h`
- ✅ `modules/web/include/web/service/zlm_service.h`
  ```cpp
  // 之前
  #include "common/service/iservice.h"
  
  // 之后
  #include "application/iservice.h"
  ```

---

#### web 测试和 API
- ✅ `modules/web/test/test_service_arch.cpp`
- ✅ `modules/web/src/api/stream_api_handler.cpp`
- ✅ `modules/web/src/api/system_api_handler.cpp`
  ```cpp
  // 之前
  #include "common/service/service_container.h"
  
  // 之后
  #include "application/service_container.h"
  ```

---

### 3. 更新 CMakeLists.txt（4个文件）

#### 主 CMakeLists.txt
```cmake
# 之前
add_subdirectory(modules/common)

# 之后
add_subdirectory(modules/application)
```

```cmake
# 主程序依赖
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        # ...
        application_lib  # 之前是 common_lib
        # ...
)
```

---

#### web/CMakeLists.txt
```cmake
target_link_libraries(web_lib
    PUBLIC
        application_lib   # 之前是 common_lib
        # ...
)
```

---

#### zlmediakit/CMakeLists.txt
```cmake
target_link_libraries(zlmediakit_lib
    PUBLIC
        application_lib   # 之前是 common_lib
        # ...
)
```

---

#### application/CMakeLists.txt（新建）
```cmake
cmake_minimum_required(VERSION 3.18)
project(application_lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE APP_SOURCES "src/*.cpp")
add_library(${PROJECT_NAME} ${APP_SOURCES})

target_include_directories(${PROJECT_NAME} 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        log_lib
)
```

---

## 💡 设计原则

### 1. 分层架构

```
┌─────────────────────────┐
│   Application Layer     │  ← application 模块
│   (IService, Container) │
└────────────┬────────────┘
             │ 依赖
┌────────────┴────────────┐
│   Business Layer        │  ← 各业务模块
│   (ZLM, Web, Camera...) │
└────────────┬────────────┘
             │ 依赖
┌────────────┴────────────┐
│   Common Layer          │  ← common 模块
│   (Utils, Tools)        │
└─────────────────────────┘
```

---

### 2. 依赖倒置

```
高层模块（Application）定义接口（IService）
低层模块（ZLM, Web）实现接口
两者都依赖抽象，而不是具体实现
```

---

### 3. 单一职责

- **common 模块**: 提供通用工具
- **application 模块**: 管理应用生命周期
- **业务模块**: 实现具体业务逻辑

---

## 🎯 优势

### 1. 消除循环依赖
```
✅ application_lib 不依赖任何业务模块
✅ 业务模块只依赖 application_lib
✅ 清晰的单向依赖
```

---

### 2. 更好的模块化
```
✅ common: 纯工具，可独立使用
✅ application: 应用框架，定义规范
✅ 业务模块: 实现具体功能
```

---

### 3. 易于扩展
```cpp
// 可以轻松添加新的业务模块
class NewService : public IService {
    // ...
};

// 在 main.cpp 中注册
container.registerService<NewService>(...);
```

---

### 4. 便于测试
```cpp
// 可以单独测试 application 模块
// 可以 mock IService 进行测试
// 各模块独立测试
```

---

## ⚠️ 注意事项

### 1. common 模块的未来

目前 `common` 模块几乎是空的。将来可以放入：
- 字符串工具函数
- 文件操作工具
- 时间日期工具
- 数学计算工具
- 等...

**原则**: 只放无状态、无业务逻辑的工具函数。

---

### 2. application 模块的职责

**应该包含**:
- ✅ 服务接口定义（IService）
- ✅ 服务容器（ServiceContainer）
- ✅ 应用框架（Application）
- ✅ 信号处理（SignalHandler）

**不应该包含**:
- ❌ 具体业务逻辑
- ❌ 网络代码
- ❌ 数据库代码
- ❌ 任何依赖业务模块的代码

---

### 3. 依赖顺序

在 CMakeLists.txt 中，模块的添加顺序很重要：

```cmake
# 基础模块先添加
add_subdirectory(modules/log)
add_subdirectory(modules/net)
add_subdirectory(modules/config)

# application 模块（依赖 log）
add_subdirectory(modules/application)

# 业务模块（依赖 application）
add_subdirectory(modules/zlmediakit)
add_subdirectory(modules/web)
```

---

## 🚀 下一步

### 1. 编译验证
```bash
cd out\build\x64-Debug
cmake --build .
```

---

### 2. 运行测试
```bash
./bin/test_web_test_service_arch.exe
```

---

### 3. 清理旧文件
确认一切正常后，可以删除：
- `modules/common/include/common/service/` (已空)
- `modules/common/include/common/application.h` (已移动)
- `modules/common/include/common/signal_handler.h` (已移动)
- `modules/common/src/application.cpp` (已移动)
- `modules/common/src/signal_handler.cpp` (已移动)

---

## 📝 总结

### 核心思想

**将"应用框架"从"通用工具"中分离出来。**

- **common**: 纯工具，无业务逻辑
- **application**: 应用框架，定义规范
- **业务模块**: 实现具体功能

---

### 关键改动

1. ✅ **创建 application 模块**
2. ✅ **移动 IService, ServiceContainer, Application, SignalHandler**
3. ✅ **更新所有 include 路径**
4. ✅ **更新 CMakeLists.txt 依赖**
5. ✅ **消除循环依赖**

---

### 最终效果

```
✅ 清晰的模块层次
✅ 无循环依赖
✅ 易于维护和扩展
✅ 符合设计原则
```

---

## 🔗 相关文档

- [CIRCULAR_DEPENDENCY_FIX.md](modules/zlmediakit/CIRCULAR_DEPENDENCY_FIX.md) - 循环依赖修复
- [ZLM_SERVICE_MIGRATION.md](modules/zlmediakit/ZLM_SERVICE_MIGRATION.md) - ZLMService 迁移
- [SERVICE_MIGRATION_COMPLETE.md](modules/common/SERVICE_MIGRATION_COMPLETE.md) - Service 层迁移总结
