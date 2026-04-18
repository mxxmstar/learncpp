# 解决循环依赖问题

## 🐛 问题描述

之前的实现存在循环依赖：

```
common_lib (IService)
    ↑
zlmediakit_lib (ZLMService) → web_lib (HttpClientPoolService)
    ↑                              ↑
    └──────────────────────────────┘
         循环依赖！
```

**原因**:
- `ZLMService` 在 `initialize()` 中通过 `ServiceContainer` 获取 `HttpClientPoolService`
- 这需要 include `"web/service/httpclient_pool_service.h"`
- 导致 `zlmediakit_lib` 依赖 `web_lib`
- 但 `web_lib` 又依赖 `zlmediakit_lib`（使用 ZLMService）
- **形成循环依赖！**

---

## ✅ 解决方案

### 方案：依赖注入（Dependency Injection）

让 `ZLMService` 的构造函数直接接收 `HttpClientPool*` 指针，而不是通过 ServiceContainer 获取。

**优点**:
- ✅ 消除循环依赖
- ✅ 符合依赖倒置原则
- ✅ 更易于测试
- ✅ 更清晰的依赖关系

---

## 🔧 修改内容

### 1. 更新头文件

**文件**: `modules/zlmediakit/include/zlmediakit/service/zlm_service.h`

#### 添加前向声明
```cpp
// 前向声明，避免依赖 web 模块
namespace Net {
    class HttpClientPool;
}
```

#### 修改构造函数
```cpp
// 之前
explicit ZLMService(boost::asio::io_context& ctx, const ZlmConfig& config);

// 之后
explicit ZLMService(boost::asio::io_context& ctx, 
                   Net::HttpClientPool* http_pool,  // ← 新增参数
                   const ZlmConfig& config);
```

#### 添加成员变量
```cpp
private:
    boost::asio::io_context& ctx_;
    Net::HttpClientPool* http_pool_;  // ← 新增：由外部提供
    ZlmConfig config_;
    // ...
```

---

### 2. 更新实现文件

**文件**: `modules/zlmediakit/src/service/zlm_service.cpp`

#### 更新 include
```cpp
// 之前
#include "web/service/httpclient_pool_service.h"
#include "common/service/service_container.h"

// 之后
#include "net/httpclientpool.h"  // 直接依赖 net 模块
```

#### 更新构造函数
```cpp
ZLMService::ZLMService(boost::asio::io_context& ctx, 
                      Net::HttpClientPool* http_pool,
                      const ZlmConfig& config)
    : ctx_(ctx), http_pool_(http_pool), config_(config) {
}
```

#### 更新 initialize()
```cpp
bool ZLMService::initialize() {
    // ...
    
    // 之前：通过 ServiceContainer 获取
    auto http_pool_svc = ServiceContainer::getInstance().getService<HttpClientPoolService>();
    if (!http_pool_svc || !http_pool_svc->isInitialized()) {
        return false;
    }
    zlm_manager_ = std::unique_ptr<ZLMManager>(
        new ZLMManager(ctx_, http_pool_svc->getHttpClientPool(), config_)
    );
    
    // 之后：直接使用传入的指针
    if (!http_pool_) {
        LOG_MAIN_ERROR_AT("{}: HttpClientPool is null", getName());
        return false;
    }
    zlm_manager_ = std::unique_ptr<ZLMManager>(
        new ZLMManager(ctx_, http_pool_, config_)
    );
    
    // ...
}
```

---

### 3. 更新调用方

**文件**: `modules/web/test/test_service_arch.cpp`

```cpp
// 之前
container.registerService<zlmediakit::ZLMService>(shared_ctx, config.zlm);

// 之后
// 先获取 HttpClientPoolService
auto http_pool_svc = container.getService<HttpClientPoolService>();
if (http_pool_svc) {
    // 然后传入 HttpClientPool 指针
    container.registerService<zlmediakit::ZLMService>(
        shared_ctx, 
        http_pool_svc->getHttpClientPool(),  // ← 传入指针
        config.zlm
    );
}
```

---

## 📊 依赖关系对比

### 修改前（有循环依赖）

```
web_lib
  ├── common_lib
  ├── zlmediakit_lib
  └── net_lib

zlmediakit_lib
  ├── common_lib
  ├── web_lib  ❌ 循环依赖！
  └── net_lib
```

---

### 修改后（无循环依赖）

```
web_lib
  ├── common_lib
  ├── zlmediakit_lib
  └── net_lib

zlmediakit_lib
  ├── common_lib
  └── net_lib  ✅ 只依赖基础模块
```

**清晰的单向依赖！**

---

## 💡 设计原则

### 1. 依赖倒置原则 (DIP)

```
高层模块（ZLMService）不应该依赖低层模块（HttpClientPoolService）
两者都应该依赖抽象（HttpClientPool*）
```

---

### 2. 控制反转 (IoC)

```
之前：ZLMService 自己查找依赖（通过 ServiceContainer）
之后：依赖由外部注入（通过构造函数）
```

---

### 3. 单一职责原则 (SRP)

```
ZLMService 的职责：管理 ZLMManager 的生命周期
不应该负责查找和获取依赖
```

---

## 🎯 优势

### 1. 消除循环依赖
- ✅ 编译顺序清晰
- ✅ 链接不会出错
- ✅ 模块化更好

---

### 2. 更易于测试
```cpp
// 可以轻松 mock HttpClientPool
class MockHttpClientPool : public Net::HttpClientPool {
    // ...
};

MockHttpClientPool mock_pool;
ZLMService service(ctx, &mock_pool, config);
```

---

### 3. 更清晰的依赖关系
```cpp
// 一眼就能看出 ZLMService 需要什么
ZLMService(ctx, http_pool, config);
```

---

### 4. 灵活性更高
```cpp
// 可以使用不同的 HttpClientPool 实现
ZLMService service1(ctx, pool1, config);
ZLMService service2(ctx, pool2, config);
```

---

## ⚠️ 注意事项

### 1. 注册顺序

必须确保 `HttpClientPoolService` 在 `ZLMService` 之前注册：

```cpp
// ✅ 正确顺序
container.registerService<HttpClientPoolService>(...);
auto http_pool_svc = container.getService<HttpClientPoolService>();
container.registerService<ZLMService>(..., http_pool_svc->getHttpClientPool(), ...);

// ❌ 错误顺序
container.registerService<ZLMService>(...);  // http_pool 为 nullptr!
container.registerService<HttpClientPoolService>(...);
```

---

### 2. 生命周期管理

确保 `HttpClientPool` 的生命周期长于 `ZLMService`：

```cpp
// HttpClientPoolService 应该在 ZLMService 之前停止
// ServiceContainer 会按注册的逆序停止，所以没问题
```

---

### 3. 空指针检查

在 `initialize()` 中检查指针有效性：

```cpp
if (!http_pool_) {
    LOG_MAIN_ERROR_AT("{}: HttpClientPool is null", getName());
    return false;
}
```

---

## 🔄 其他服务的类似问题

如果其他服务也有类似的跨模块依赖，应该采用相同的解决方案：

| 服务 | 依赖 | 解决方案 |
|------|------|---------|
| **ZLMService** | HttpClientPool | ✅ 已修复：通过构造函数注入 |
| CameraService (未来) | SQLite | 建议：通过构造函数注入 |
| FFmpegService (未来) | Config | 建议：通过构造函数注入 |

---

## 📝 总结

### 核心思想

**不要通过 ServiceContainer 获取依赖，而是通过构造函数注入依赖。**

---

### 关键改动

1. ✅ **ZLMService 构造函数** - 添加 `HttpClientPool*` 参数
2. ✅ **initialize()** - 直接使用传入的指针
3. ✅ **include 路径** - 从 `web/service/` 改为 `net/`
4. ✅ **调用方** - 先获取服务，再传入指针

---

### 最终效果

```
✅ 无循环依赖
✅ 清晰的模块边界
✅ 易于测试和维护
✅ 符合设计原则
```

---

## 🔗 相关文档

- [ZLM_SERVICE_MIGRATION.md](modules/zlmediakit/ZLM_SERVICE_MIGRATION.md) - ZLMService 迁移说明
- [SERVICE_MIGRATION_COMPLETE.md](modules/common/SERVICE_MIGRATION_COMPLETE.md) - Service 层迁移总结
