# Web 模块重命名为 API 模块

## ✅ 完成的工作

### 1. 目录重命名
- ✅ `modules/web/` → `modules/api/`

### 2. CMakeLists.txt 更新
- ✅ 主 CMakeLists.txt: `add_subdirectory(modules/api)`
- ✅ api/CMakeLists.txt: 项目名改为 `api_module`，库名改为 `api_lib`
- ✅ api/test/CMakeLists.txt: 测试可执行文件改为 `test_api_*`

### 3. 头文件路径更新
- ✅ `#include "web/api/xxx.h"` → `#include "api/api/xxx.h"`
- ✅ `#include "web/service/xxx.h"` → `#include "service/xxx.h"` (Service 已迁移)
- ✅ app_with_framework.cpp 中的引用已更新

### 4. 依赖关系更新
- ✅ 主 CMakeLists.txt: `web_lib` → `api_lib`
- ✅ api 模块内部的文件引用已更新

### 5. 旧文件清理
- ✅ 删除 `modules/web/` 目录

---

## 📊 新的架构

```
modules/
├── api/                 ← Web API 业务层（原 web 模块）
│   ├── include/api/
│   │   └── api/        ← API Handlers
│   │       ├── stream_api_handler.h
│   │       ├── camera_api_handler.h
│   │       └── system_api_handler.h
│   └── src/
│       └── api/        ← API 实现
│
├── service/             ← Service 实现层
│   ├── http_server/    ← HttpServerService
│   ├── http_client/    ← HttpClientPoolService
│   └── zlm/            ← ZLMService
│
├── net/http_server/     ← HTTP 服务器底层实现
│   ├── http_server.h
│   ├── http_session.h
│   └── http_router.h   ← 路由分发（属于 http_server）
```

---

## 🎯 职责划分

### net/http_server（网络层）
- HTTP 协议解析
- TCP 连接管理
- Session 管理
- **HttpRouter**: 路由分发机制

### api（业务 API 层）
- `/stream/*` - 流管理 API
- `/camera/*` - 摄像头管理 API  
- `/system/*` - 系统管理 API
- 业务逻辑处理
- API 路由注册

### service（服务层）
- HttpServerService - HTTP 服务器服务
- HttpClientPoolService - HTTP 客户端池服务
- ZLMService - ZLMediaKit 服务

---

## 🔧 为什么 HttpRouter 不独立？

**原因**：

1. **HttpRouter 是 HTTP Server 的核心组件**
   - 处理 HTTP 请求的路由分发
   - 依赖 Boost.Beast HTTP
   - 与 HttpSession 紧密配合

2. **没有独立复用的价值**
   - Router 专用于 HTTP 服务器
   - 不是通用组件
   - 独立会增加复杂度

3. **正确的分层**
   ```
   API Handlers (api 模块)
       ↑ 使用
   HttpRouter (net/http_server)
       ↑ 使用
   HttpServer (net/http_server)
   ```

---

## ⚠️ 注意事项

1. **编译验证**：需要重新配置 CMake 并编译
2. **测试更新**：如果有其他文件引用了 `web/`，需要手动更新
3. **文档更新**：README 等文档中的路径需要更新

---

## 🚀 下一步

1. **编译验证**
   ```bash
   cd out\build\x64-Debug
   cmake ..\..\..
   cmake --build .
   ```

2. **修复编译错误**
   - 检查是否还有遗漏的 `#include "web/...`
   - 检查链接依赖是否正确

3. **功能测试**
   - 测试所有 API 端点
   - 确保路由正常工作

---

## 📝 总结

- ✅ **web 模块已重命名为 api 模块**
- ✅ **HttpRouter 保留在 http_server 中**（合理的设计）
- ✅ **架构更清晰**：net（网络层）→ api（业务层）→ service（服务层）
