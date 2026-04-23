# 完整架构重构方案

## 🎯 重构目标

将项目重构为更清晰的分层架构：

```
modules/
├── common/              ← 通用基础模块（log, config, service接口）
├── service/             ← 所有 Service 实现层
├── net/                 ← 网络底层库（已拆分）
├── zlmediakit/          ← ZLMediaKit 封装
├── web/                 ← Web API 层
├── camera/              ← Camera 业务逻辑
├── application/         ← Application 框架
└── ...
```

---

## ✅ 已完成的工作

### 1. Net 模块拆分 ✅
- http_client/
- http_server/
- tcp_server/
- websocket/
- io_context_pool/

### 2. 目录结构准备 ✅
- common/include/common/log/ ← 已复制
- common/include/common/config/ ← 已复制
- service/http_server/ ← 已创建
- service/http_client/ ← 已创建
- service/zlm/ ← 已创建

---

## 📋 待完成工作

### 阶段 1：更新头文件路径（log & config）

#### 1.1 更新 log 模块内部引用
文件：`common/include/common/log/logger.h`, `logmanager.h`
```cpp
// 之前
#include "log/logger.h"

// 之后  
#include "common/log/logger.h"
```

#### 1.2 更新 config 模块内部引用
文件：`common/include/common/config/common_config.h`
```cpp
// 之前
#include "config/xxx.h"

// 之后
#include "common/config/xxx.h"
```

#### 1.3 更新所有使用 log 和 config 的文件
需要批量替换：
- `#include "log/` → `#include "common/log/`
- `#include "config/` → `#include "common/config/`

---

### 阶段 2：迁移 Service 到 service 模块

#### 2.1 HttpServerService
**从**: `modules/web/include/web/service/http_server_service.h`
**到**: `modules/service/http_server/include/service/http_server/http_server_service.h`

**从**: `modules/web/src/service/http_server_service.cpp`
**到**: `modules/service/http_server/src/http_server_service.cpp`

#### 2.2 HttpClientPoolService
**从**: `modules/web/include/web/service/httpclient_pool_service.h`
**到**: `modules/service/http_client/include/service/http_client/http_client_pool_service.h`

**从**: `modules/web/src/service/httpclient_pool_service.cpp`
**到**: `modules/service/http_client/src/http_client_pool_service.cpp`

#### 2.3 ZLMService
**从**: `modules/zlmediakit/include/zlmediakit/service/zlm_service.h`
**到**: `modules/service/zlm/include/service/zlm/zlm_service.h`

**从**: `modules/zlmediakit/src/service/zlm_service.cpp`
**到**: `modules/service/zlm/src/zlm_service.cpp`

---

### 阶段 3：创建 service 模块的 CMakeLists.txt

#### 3.1 主 service/CMakeLists.txt
```cmake
add_subdirectory(http_server)
add_subdirectory(http_client)
add_subdirectory(zlm)
```

#### 3.2 各子模块 CMakeLists.txt
- service/http_server/CMakeLists.txt
- service/http_client/CMakeLists.txt
- service/zlm/CMakeLists.txt

---

### 阶段 4：更新所有依赖关系

#### 4.1 更新 web 模块
- 移除对 web/service/ 的引用
- 添加对 service/http_server/ 和 service/http_client/ 的依赖

#### 4.2 更新 zlmediakit 模块
- 移除对 zlmediakit/service/ 的引用
- 添加对 service/zlm/ 的依赖

#### 4.3 更新 application 模块
- 更新 IService 的路径（仍在 common 中）

#### 4.4 更新 app_with_framework.cpp
- 更新所有 Service 的 include 路径

---

### 阶段 5：删除旧文件

确认编译通过后，删除：
- modules/log/ （已移动到 common）
- modules/config/ （已移动到 common）
- modules/web/include/web/service/ （已移动到 service）
- modules/web/src/service/ （已移动到 service）
- modules/zlmediakit/include/zlmediakit/service/ （已移动到 service）
- modules/zlmediakit/src/service/ （已移动到 service）

---

## 🔧 执行策略

### 方案 A：一次性完成（推荐）
1. 批量更新所有头文件路径
2. 移动所有 Service 文件
3. 创建所有 CMakeLists.txt
4. 清理并重新编译
5. 修复编译错误

**优点**：快速完成
**缺点**：如果出错，调试困难

### 方案 B：分步进行
1. 先处理 log 和 config → 编译验证
2. 再迁移 HttpServerService → 编译验证
3. 再迁移 HttpClientPoolService → 编译验证
4. 最后迁移 ZLMService → 编译验证

**优点**：每步都可验证
**缺点**：耗时长

---

## 💡 建议

鉴于这是一个大规模重构，我建议：

1. **先备份当前代码**（git commit）
2. **采用方案 B**（分步进行），每步都编译验证
3. **创建详细的检查清单**，确保没有遗漏

你希望我现在开始执行吗？选择哪个方案？
