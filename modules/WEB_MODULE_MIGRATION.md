# API 和 Service 模块合并为 Web 模块 - 迁移完成

## 📋 迁移概述

成功将 `modules/api` 和 `modules/service` 两个模块合并为 `modules/web`，消除了循环依赖问题。

## ✅ 完成的工作

### 1. 目录结构创建
- ✅ 创建 `modules/web/include/web/api/` - API 头文件
- ✅ 创建 `modules/web/include/web/service/` - Service 头文件
- ✅ 创建 `modules/web/src/api/` - API 源文件
- ✅ 创建 `modules/web/src/service/` - Service 源文件
- ✅ 创建 `modules/web/lib/` - 库文件输出目录
- ✅ 创建 `modules/web/test/` - 测试文件目录

### 2. 文件迁移
- ✅ 迁移 api 模块的 3 个头文件到 `web/include/web/api/`
- ✅ 迁移 service 模块的 5 个头文件到 `web/include/web/service/`
- ✅ 迁移 api 模块的 3 个源文件到 `web/src/api/`
- ✅ 迁移 service 模块的 3 个源文件到 `web/src/service/`
- ✅ 迁移测试文件到 `web/test/`

### 3. CMake 配置
- ✅ 创建 `modules/web/CMakeLists.txt`
- ✅ 创建 `modules/web/test/CMakeLists.txt`
- ✅ 更新根目录 `CMakeLists.txt`：
  - 移除 `add_subdirectory(modules/api)`
  - 移除 `add_subdirectory(modules/service)`
  - 添加 `add_subdirectory(modules/web)`
  - 将 `api_lib` 和 `service_lib` 替换为 `web_lib`

### 4. 头文件引用更新
更新了以下文件中的 include 路径：

**Web 模块内部文件：**
- ✅ `web/include/web/service/service_container.h`
- ✅ `web/include/web/service/zlm_service.h`
- ✅ `web/include/web/service/httpclient_pool_service.h`
- ✅ `web/include/web/service/http_server_service.h`
- ✅ `web/src/api/stream_api_handler.cpp`
- ✅ `web/src/api/system_api_handler.cpp`
- ✅ `web/src/api/api_router_registrar.cpp`
- ✅ `web/src/service/zlm_service.cpp`
- ✅ `web/src/service/httpclient_pool_service.cpp`
- ✅ `web/src/service/http_server_service.cpp`
- ✅ `web/test/test_service_arch.cpp`

**引用变化：**
- `#include "api/xxx.h"` → `#include "web/api/xxx.h"`
- `#include "service/xxx.h"` → `#include "web/service/xxx.h"`

### 5. 编译验证
- ✅ CMake 配置成功
- ✅ web_lib 编译成功
- ✅ 主程序 MySelfContainedApp.exe 编译成功
- ✅ 生成的库文件：`modules/web/lib/Debug/web_lib.lib`

## 📊 迁移前后对比

### 之前（存在循环依赖）
```
modules/api/          ← 依赖 service_lib (循环依赖!)
└── lib/api_lib.lib

modules/service/      ← 依赖 api_lib (循环依赖!)
└── lib/service_lib.lib
```

### 现在（消除循环依赖）
```
modules/web/          ← 统一的 Web 模块，无循环依赖
├── include/web/
│   ├── api/          # API 路由和处理
│   └── service/      # 服务管理
├── src/
│   ├── api/
│   └── service/
└── lib/web_lib.lib
```

## 🔧 依赖关系

### Web 模块的依赖
```cmake
target_link_libraries(web_lib
    PUBLIC
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
        log_lib
        net_lib
        zlmediakit_lib
        config_lib
)
```

### 依赖层次
```
基础层:
  log_lib

第二层:
  net_lib → log_lib
  config_lib → log_lib

第三层:
  zlmediakit_lib → log_lib + net_lib

第四层 (最高层):
  web_lib → log_lib + net_lib + zlmediakit_lib + config_lib
```

## 💡 使用说明

### 在新的代码中使用 Web 模块

```cpp
// API 相关
#include "web/api/api_router_registrar.h"
#include "web/api/stream_api_handler.h"
#include "web/api/system_api_handler.h"

// Service 相关
#include "web/service/iservice.h"
#include "web/service/service_container.h"
#include "web/service/http_server_service.h"
#include "web/service/zlm_service.h"
#include "web/service/httpclient_pool_service.h"

// CMakeLists.txt 中链接
target_link_libraries(your_module PRIVATE web_lib)
```

### 编译选项

```powershell
# 默认编译（测试禁用）
cmake ..
cmake --build . --config Debug

# 启用 Web 模块测试
cmake .. -DBUILD_WEB_TESTS=ON
cmake --build . --config Debug

# 运行测试
.\modules\web\test\bin\test_web_service_arch.exe
```

## 📝 注意事项

1. **旧的备份目录已清理**：
   - ✅ `modules/api/` - 已删除
   - ✅ `modules/service/` - 已删除
   - ✅ `include/api/` - 已删除
   - ✅ `include/service/` - 已删除
   - ✅ `src/api/` - 已删除
   - ✅ `src/service/` - 已删除

2. **头文件路径变更**：
   - 所有引用需要添加 `web/` 前缀
   - 例如：`"api/xxx.h"` → `"web/api/xxx.h"`

3. **测试文件**：
   - 测试可执行文件命名：`test_web_*.exe`
   - 输出目录：`modules/web/test/bin/`

## 🎯 优势

1. **消除循环依赖**：api 和 service 不再相互依赖
2. **提高内聚性**：Web 相关的功能集中在一个模块
3. **简化依赖管理**：减少模块数量，降低维护成本
4. **清晰的职责划分**：
   - API 层：HTTP 路由和请求处理
   - Service 层：业务逻辑和服务生命周期管理

## 📚 相关文档

- [Web 模块 README](modules/web/README.md)
- [API_CONFIG_REFACTORING.md](modules/API_CONFIG_REFACTORING.md) - 之前的重构文档
- [SERVICE_REFACTORING.md](modules/service/REFACTORING.md) - Service 模块重构文档

---

**迁移日期**: 2026-04-12  
**迁移状态**: ✅ 完成  
**编译状态**: ✅ 成功  
**旧模块清理**: ✅ 已完成（2026-04-12）
