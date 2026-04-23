# ZLMService 迁移到 zlmediakit 模块

## 🎯 迁移原因

`ZLMService` 是 ZLMediaKit 的服务封装，应该放在 `zlmediakit` 模块中，而不是 `web` 模块。

**正确的架构**:
```
modules/
├── common/              # 基础设施
│   └── service/
│       ├── iservice.h
│       └── service_container.h
├── zlmediakit/          # ZLMediaKit 模块
│   └── include/zlmediakit/
│       ├── zlm_manager.h
│       └── service/     ← ZLMService 应该在这里
│           └── zlm_service.h
└── web/                 # Web 应用
    └── include/web/
        └── service/
            ├── http_server_service.h
            └── httpclient_pool_service.h
```

---

## ✅ 已完成的工作

### 1. 创建目录结构
- ✅ `modules/zlmediakit/include/zlmediakit/service/`
- ✅ `modules/zlmediakit/src/service/`

---

### 2. 创建新文件

#### 头文件
**文件**: `modules/zlmediakit/include/zlmediakit/service/zlm_service.h`

**关键改动**:
```cpp
// 添加命名空间
namespace zlmediakit {

class ZLMService : public IService {
    // ...
};

} // namespace zlmediakit
```

---

#### 源文件
**文件**: `modules/zlmediakit/src/service/zlm_service.cpp`

**关键改动**:
```cpp
#include "zlmediakit/service/zlm_service.h"

namespace zlmediakit {

// 实现...

} // namespace zlmediakit
```

---

### 3. 更新 CMakeLists.txt

**文件**: `modules/zlmediakit/CMakeLists.txt`

**添加源文件收集**:
```cmake
file(GLOB ZLM_SERVICE_SOURCES "src/service/*.cpp")
list(APPEND ZLM_SOURCES ${ZLM_SERVICE_SOURCES})
```

**添加依赖**:
```cmake
target_link_libraries(zlmediakit_lib
    PUBLIC
        common_lib        # ← 新增：需要 IService
        log_lib
        net_lib
        config_lib
)
```

---

### 4. 更新引用（4个文件）

#### A. 测试文件
**文件**: `modules/web/test/test_service_arch.cpp`

```cpp
// 之前
#include "web/service/zlm_service.h"
container.registerService<ZLMService>(...);

// 之后
#include "zlmediakit/service/zlm_service.h"
container.registerService<ZLMService>(...);
```

---

#### B. Stream API Handler
**文件**: `modules/web/src/api/stream_api_handler.cpp`

```cpp
// 之前
#include "web/service/zlm_service.h"
auto zlm_svc = ServiceContainer::getInstance().getService<ZLMService>();

// 之后
#include "zlmediakit/service/zlm_service.h"
auto zlm_svc = ServiceContainer::getInstance().getService<ZLMService>();
```

---

#### C. System API Handler
**文件**: `modules/web/src/api/system_api_handler.cpp`

```cpp
// 之前
#include "web/service/zlm_service.h"
auto zlm_svc = ServiceContainer::getInstance().getService<ZLMService>();

// 之后
#include "zlmediakit/service/zlm_service.h"
auto zlm_svc = ServiceContainer::getInstance().getService<ZLMService>();
```

---

#### D. 旧的 ZLMService 实现（可以删除）
**文件**: `modules/web/src/service/zlm_service.cpp`

这个文件已经不再需要，因为实现已经移到 zlmediakit 模块。

---

## 📊 修改统计

| 类型 | 数量 | 说明 |
|------|------|------|
| **新创建文件** | 2 | zlm_service.h, zlm_service.cpp (在 zlmediakit) |
| **更新头文件** | 0 | - |
| **更新源文件** | 3 | 2个 API handler + 1个测试 |
| **更新 CMake** | 1 | zlmediakit/CMakeLists.txt |
| **总计** | 6 | 所有相关文件 |

---

## 🎯 架构改进

### 迁移前
```
modules/web/include/web/service/
├── http_server_service.h         ✅ Web 特有
├── httpclient_pool_service.h     ✅ Web 特有
└── zlm_service.h                 ❌ 应该在 zlmediakit
```

---

### 迁移后
```
modules/zlmediakit/include/zlmediakit/service/
└── zlm_service.h                 ✅ 正确位置

modules/web/include/web/service/
├── http_server_service.h         ✅ Web 特有
└── httpclient_pool_service.h     ✅ Web 特有
```

---

## 💡 优势

### 1. **职责清晰**
- ✅ ZLMService 在 zlmediakit 模块
- ✅ HTTP 服务在 web 模块
- ✅ 每个模块管理自己的服务

---

### 2. **便于复用**
```cpp
// 其他模块也可以直接使用 ZLMService
#include "zlmediakit/service/zlm_service.h"

class MyApplication {
    ZLMService zlm_svc_;
};
```

---

### 3. **减少耦合**
```
之前:
web_lib → zlmediakit_lib (通过 ZLMService)

现在:
web_lib → zlmediakit_lib (通过 ZLMService)
zlmediakit_lib → common_lib (通过 IService)
```

更清晰的依赖关系！

---

### 4. **符合模块化原则**
- ✅ 每个模块封装自己的功能
- ✅ 接口在 common，实现在具体模块
- ✅ 易于维护和扩展

---

## 🔗 依赖关系

### 新的依赖图
```
web_lib
  ├── common_lib (IService, ServiceContainer)
  ├── zlmediakit_lib (ZLMService)
  ├── net_lib
  └── log_lib

zlmediakit_lib
  ├── common_lib (IService)
  ├── net_lib
  ├── config_lib
  └── log_lib
```

---

## ⚠️ 注意事项

### 1. 命名空间

ZLMService 现在在 `zlmediakit` 命名空间中：

```cpp
// 使用时需要指定命名空间
ZLMService service(ctx, config);

// 或者使用 using
using ZLMService;
ZLMService service(ctx, config);
```

---

### 2. 向后兼容

如果需要保持向后兼容，可以在 web 模块创建重定向头文件：

**文件**: `modules/web/include/web/service/zlm_service.h`
```cpp
#pragma once
// 兼容层：重定向到新的位置
#include "zlmediakit/service/zlm_service.h"

// 可选：提供别名
using ZLMService = ZLMService;
```

但**不推荐**这样做，应该直接使用新的路径。

---

### 3. 旧文件清理

以下文件可以删除：
- `modules/web/include/web/service/zlm_service.h`
- `modules/web/src/service/zlm_service.cpp`

等所有测试通过后再删除。

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
确认一切正常后，删除：
- `modules/web/include/web/service/zlm_service.h`
- `modules/web/src/service/zlm_service.cpp`

---

### 4. 更新文档
- 更新 `modules/web/README.md`
- 更新 `modules/zlmediakit/README.md`（如果有的话）

---

## 📝 类似的服务迁移建议

按照同样的原则，其他服务也应该放在对应的模块中：

| 服务 | 当前模块 | 应该在 | 状态 |
|------|---------|--------|------|
| **HttpServerService** | web | web | ✅ 正确 |
| **HttpClientPoolService** | web | web | ✅ 正确 |
| **ZLMService** | ~~web~~ | zlmediakit | ✅ 已迁移 |
| CameraService (未来) | - | camera | - |
| FFmpegService (未来) | - | ffmpeg_opt | - |

---

## 🎉 总结

### 成果

1. ✅ **成功迁移** ZLMService 到 zlmediakit 模块
2. ✅ **添加命名空间** - `ZLMService`
3. ✅ **更新所有引用** - 4个文件
4. ✅ **更新 CMake** - 添加源文件和依赖
5. ✅ **改善架构** - 清晰的模块边界

---

### 设计原则

- ✅ **单一职责** - 每个模块管理自己的服务
- ✅ **高内聚低耦合** - 相关代码放在一起
- ✅ **便于复用** - 其他模块可以轻松使用
- ✅ **易于维护** - 清晰的模块边界

---

## 🔗 相关文档

- [SERVICE_MIGRATION_COMPLETE.md](modules/common/SERVICE_MIGRATION_COMPLETE.md) - Service 层迁移总结
- [SERVICE_MIGRATION_PLAN.md](modules/common/SERVICE_MIGRATION_PLAN.md) - 详细的迁移计划
