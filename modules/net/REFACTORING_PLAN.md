# Net 模块渐进式拆分方案

## 📋 目标

将臃肿的 `modules/net/` 拆分为多个独立子模块，解决循环依赖问题，提高代码可维护性。

---

## 🎯 最终架构

```
modules/
├── net/
│   ├── http_client/      ← HTTP 客户端（独立模块）
│   ├── http_server/      ← HTTP 服务器
│   ├── tcp_server/       ← TCP 服务器
│   ├── websocket/        ← WebSocket
│   └── io_context_pool/  ← IO 上下文池
```

---

## 🚀 分阶段实施计划

### 阶段 1：准备阶段（当前已完成 ✅）

**目标**：创建基础目录结构

**完成事项**：
- ✅ 创建 `net/http_client/` 目录结构
- ✅ 解决 ZLMService 循环依赖（使用手动注入）

**下一步**：迁移 http_client 相关文件

---

### 阶段 2：迁移 HTTP Client（2-3天）

#### 2.1 移动文件

```bash
# 从 modules/net/ 移动到 modules/net/http_client/
include/net/httpclient.h         → include/net/http_client/http_client.h
include/net/httpclientpool.h     → include/net/http_client/http_client_pool.h
src/httpclient.cpp               → src/http_client.cpp
src/httpclientpool.cpp           → src/http_client_pool.cpp
```

#### 2.2 创建 CMakeLists.txt

```cmake
# modules/net/http_client/CMakeLists.txt
project(http_client_lib)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

add_library(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 依赖
find_package(Boost REQUIRED COMPONENTS json)
target_link_libraries(${PROJECT_NAME} PUBLIC
    Boost::json
    Boost::asio
    log_lib  # 日志
)
```

#### 2.3 更新主 CMakeLists.txt

```cmake
# modules/net/CMakeLists.txt
add_subdirectory(http_client)  # 添加子模块

# 其他子模块后续添加
# add_subdirectory(http_server)
# add_subdirectory(tcp_server)
# ...
```

#### 2.4 更新引用路径

**需要修改的文件**：
```cpp
// 之前
#include "net/httpclient.h"
#include "net/httpclientpool.h"

// 之后
#include "net/http_client/http_client.h"
#include "net/http_client/http_client_pool.h"
```

**影响范围**：
- modules/zlmediakit/src/service/zlm_service.cpp
- modules/web/src/service/httpclient_pool_service.cpp
- 其他使用 HttpClientPool 的地方

#### 2.5 验证编译

```bash
cd out\build\x64-Debug
cmake --build .
```

**预期结果**：✅ 编译成功，无循环依赖

---

### 阶段 3：迁移其他组件（每个组件 1-2 天）

按以下顺序逐步迁移：

#### 3.1 HTTP Server
```
include/net/httpserver.h       → http_server/include/
include/net/httprouter.h       → http_server/include/
include/net/httpsession.h      → http_server/include/
src/httpserver.cpp             → http_server/src/
src/httprouter.cpp             → http_server/src/
src/httpsession.cpp            → http_server/src/
```

#### 3.2 TCP Server
```
include/net/tcpserver.h        → tcp_server/include/
include/net/tcpsession.h       → tcp_server/include/
include/net/session.h          → tcp_server/include/
include/net/databuffer.h       → tcp_server/include/
src/tcpserver.cpp              → tcp_server/src/
src/tcpsession.cpp             → tcp_server/src/
src/session.cpp                → tcp_server/src/
src/databuffer.cpp             → tcp_server/src/
```

#### 3.3 WebSocket
```
include/net/websocket*.h       → websocket/include/
src/websocket*.cpp             → websocket/src/
```

#### 3.4 IO Context Pool
```
include/net/asio_io_context_pool.h → io_context_pool/include/
src/asio_io_context_pool.cpp       → io_context_pool/src/
```

**每个组件迁移步骤**：
1. 移动文件到新目录
2. 创建 CMakeLists.txt
3. 更新引用路径
4. 更新主 CMakeLists.txt
5. 编译验证

---

### 阶段 4：清理和优化（1天）

#### 4.1 删除旧文件

确认所有引用都已更新后，删除 `modules/net/` 根目录下的旧文件。

#### 4.2 更新文档

- 更新 README.md
- 更新架构文档
- 更新依赖关系图

#### 4.3 优化依赖关系

检查是否有不必要的依赖，进一步优化。

---

## 📊 依赖关系图

### 当前（迁移前）

```
┌─────────────────────────────┐
│       modules/net/          │
│  (所有网络相关代码混在一起)   │
└─────────────────────────────┘
         ↑         ↑
         │         │
    zlmediakit   web
    (循环依赖!)  (依赖 net)
```

### 目标（迁移后）

```
┌──────────────────────────────────────┐
│      modules/net/http_client/        │  ← 独立模块
│  - http_client.h/cpp                 │
│  - http_client_pool.h/cpp            │
└──────────────────────────────────────┘
         ↑                    ↑
         │                    │
    zlmediakit            web/http_client_pool_service
    (只依赖接口)          (实现服务层)

┌──────────────────────────────────────┐
│      modules/net/http_server/        │
│  - http_server.h/cpp                 │
│  - http_router.h/cpp                 │
│  - http_session.h/cpp                │
└──────────────────────────────────────┘
         ↑
         │
    web/http_server_service

其他模块类似...
```

---

## ⚠️ 注意事项

### 1. 每次迁移都要编译通过

```bash
# 每完成一个组件的迁移，立即编译验证
cd out\build\x64-Debug
cmake ..
cmake --build .
```

### 2. 保持向后兼容（可选）

如果需要平滑过渡，可以在旧位置保留头文件，只是转发到新位置：

```cpp
// modules/net/include/net/httpclient.h (旧位置)
#pragma once
#include "net/http_client/http_client.h"  // 转发到新位置
```

### 3. 更新测试

迁移完成后，更新所有测试文件的 include 路径。

### 4. 文档同步

- 更新 CMakeLists.txt 注释
- 更新架构文档
- 更新开发者指南

---

## 🎯 收益

### 1. 解决循环依赖
```
zlmediakit 不再依赖 web 模块
web 可以依赖 zlmediakit（单向依赖）
```

### 2. 提高可维护性
```
每个模块职责单一
易于理解和修改
```

### 3. 支持独立测试
```
可以单独编译和测试 http_client
不需要编译整个 net 模块
```

### 4. 支持选择性使用
```
项目可以只使用 http_client
不需要链接整个 net 模块
```

---

## 📅 时间估算

| 阶段 | 工作内容 | 预计时间 |
|------|---------|---------|
| 阶段 1 | 准备阶段 | ✅ 已完成 |
| 阶段 2 | 迁移 HTTP Client | 2-3 天 |
| 阶段 3 | 迁移其他组件 | 4-8 天 |
| 阶段 4 | 清理优化 | 1 天 |
| **总计** | | **7-12 天** |

---

## 🚦 开始执行

### 立即可以做的（阶段 2）

1. **移动 http_client 文件**
2. **创建 CMakeLists.txt**
3. **更新引用路径**
4. **编译验证**

