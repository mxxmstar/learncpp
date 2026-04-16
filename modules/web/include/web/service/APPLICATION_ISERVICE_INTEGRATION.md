# Application 与 IService 整合设计

## 📋 概述

本文档说明 `Application` 如何统一管理所有实现 `IService` 接口的模块。

---

## 🎯 核心设计理念

### 1. IService - 统一的模块接口标准 ✅

**保留！** `IService` 是优秀的接口设计，作为所有模块的统一标准：

```cpp
class IService {
public:
    virtual bool initialize() = 0;   // 初始化
    virtual bool start() = 0;        // 启动
    virtual void stop() = 0;         // 停止
    virtual const char* getName() const = 0;  // 服务名称
    virtual bool isRunning() const = 0;       // 运行状态
    virtual bool isInitialized() const = 0;   // 初始化状态
};
```

**优势：**
- ✅ 统一的生命周期管理接口
- ✅ 多态支持，灵活扩展
- ✅ 类型安全（dynamic_cast）
- ✅ 已广泛应用于各个模块

---

### 2. Application - 统一的应用框架 ✅

**增强！** `Application` 直接管理所有 `IService` 服务：

```cpp
class Application {
public:
    // 注册 IService 服务
    template<typename T, typename... Args>
    bool registerService(Args&&... args);
    
    // 获取服务
    template<typename T>
    T* getService();
    
    std::shared_ptr<IService> getService(const std::string& name) const;
    
    // 自动管理生命周期
    int run();  // initialize → start → wait → stop
};
```

**职责：**
- ✅ 信号处理（跨平台）
- ✅ 配置管理
- ✅ 日志系统
- ✅ **直接管理 IService 服务**
- ✅ 依赖注入容器（任意类型）
- ✅ 生命周期自动化

---

### 3. ServiceContainer - 已废弃 ❌

**移除！** `ServiceContainer` 的功能已由 `Application` 接管。

**原因：**
- ❌ 功能与 Application 重叠
- ❌ 增加架构复杂度
- ❌ 不必要的中间层

---

## 🏗️ 新架构

### 扁平化架构

```
Application (common 模块)
    ↓ 直接管理所有 IService 服务
├─ HttpServerService      (web 模块)
├─ HttpClientPoolService  (net 模块)  
├─ ZLMService            (zlmediakit 模块)
├─ VideoPipelineService  (videopipeline 模块)
└─ AlgorithmService      (alg 模块)
```

### 对比旧架构

**旧架构（复杂）：**
```
Application
  ↓
ServiceContainer (web 模块)
  ↓
IService 服务
```

**新架构（简洁）：**
```
Application
  ↓
IService 服务
```

---

## 💻 使用示例

### 示例 1：注册和管理 IService 服务

```cpp
#include "common/application.h"
#include "web/service/http_server_service.h"
#include "net/httpclient_pool_service.h"
#include "videopipeline/video_pipeline_service.h"

int main() {
    auto& app = Application::getInstance();
    
    // 1. 初始化基础功能
    app.initLogger("logs", "info");
    app.loadConfig("config.yaml");
    
    // 2. 注册 IService 服务（自动管理生命周期）
    app.registerService<HttpServerService>(8080);
    app.registerService<HttpClientPoolService>();
    app.registerService<VideoPipelineService>(io_ctx, config);
    
    // 3. 运行（自动调用 initialize/start/stop）
    return app.run();
}
```

**执行流程：**
```
Application::run()
  ↓
initialize()
  ├─ HttpServerService::initialize()
  ├─ HttpClientPoolService::initialize()
  └─ VideoPipelineService::initialize()
  ↓
start()
  ├─ HttpServerService::start()      ← 监听 8080
  ├─ HttpClientPoolService::start()
  └─ VideoPipelineService::start()   ← 开始拉流
  ↓
等待 Ctrl+C 信号
  ↓
stop() （逆序）
  ├─ VideoPipelineService::stop()
  ├─ HttpClientPoolService::stop()
  └─ HttpServerService::stop()
```

---

### 示例 2：混合使用 IService 和普通服务

```cpp
#include "common/application.h"

int main() {
    auto& app = Application::getInstance();
    
    // 1. 注册 IService 服务（自动生命周期管理）
    app.registerService<HttpServerService>(8080);
    app.registerService<ZLMService>();
    
    // 2. 注册普通服务（手动管理）
    auto io_ctx = std::make_shared<boost::asio::io_context>();
    app.registerService<boost::asio::io_context>("io_context", *io_ctx);
    
    PipelineConfig config;
    app.registerService<VideoPipeline>("pipeline", *io_ctx, config);
    
    // 3. 自定义生命周期回调
    app.onStart([&app]() {
        // 手动启动非 IService 服务
        auto pipeline = app.getService<VideoPipeline>("pipeline");
        return pipeline->start();
    });
    
    app.onStop([&app]() {
        // 手动停止非 IService 服务
        auto pipeline = app.getService<VideoPipeline>("pipeline");
        if (pipeline) pipeline->stop();
        
        auto io_ctx = app.getService<boost::asio::io_context>("io_context");
        if (io_ctx) io_ctx->stop();
    });
    
    // 4. 运行
    return app.run();
}
```

---

### 示例 3：在 IService 中访问其他服务

```cpp
class VideoPipelineService : public IService {
public:
    bool initialize() override {
        // 可以访问 Application 中的其他服务
        auto& app = Application::getInstance();
        auto zlm_service = app.getService<ZLMService>();
        
        if (zlm_service) {
            // 使用 ZLM 服务
            zlm_service->addStreamProxy(...);
        }
        
        return true;
    }
    
    bool start() override {
        // 启动视频流水线
        return pipeline_->start();
    }
    
    void stop() override {
        pipeline_->stop();
    }
    
    const char* getName() const override { return "VideoPipelineService"; }
    bool isRunning() const override { return pipeline_->isRunning(); }
    bool isInitialized() const override { return initialized_; }
    
private:
    std::unique_ptr<VideoPipeline> pipeline_;
    bool initialized_ = false;
};
```

---

## 🔧 迁移指南

### 从 ServiceContainer 迁移到 Application

#### 之前的代码（使用 ServiceContainer）

```cpp
#include "web/service/service_container.h"

int main() {
    auto& container = ServiceContainer::getInstance();
    
    container.registerService<HttpServerService>(8080);
    container.registerService<ZLMService>();
    
    container.initializeAll();
    container.startAll();
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    container.stopAll();
    return 0;
}
```

#### 迁移后的代码（使用 Application）

```cpp
#include "common/application.h"

int main() {
    auto& app = Application::getInstance();
    
    // 初始化
    app.initLogger("logs", "info");
    
    // 注册服务（更简洁）
    app.registerService<HttpServerService>(8080);
    app.registerService<ZLMService>();
    
    // 运行（自动管理生命周期 + 信号处理）
    return app.run();
}
```

**改进：**
- ✅ 代码更简洁（减少 50% 代码量）
- ✅ 自动信号处理（Ctrl+C 优雅退出）
- ✅ 自动生命周期管理
- ✅ 可以添加配置、日志等功能

---

## 📊 功能对比

| 特性 | ServiceContainer（旧） | Application（新） |
|------|----------------------|------------------|
| **管理 IService** | ✅ | ✅ |
| **信号处理** | ❌ | ✅ |
| **配置管理** | ❌ | ✅ |
| **日志集成** | ❌ | ✅ |
| **依赖注入** | ❌ | ✅（任意类型） |
| **回调机制** | ❌ | ✅（onInit/onStart/onStop） |
| **跨平台** | ❌ | ✅ |
| **架构复杂度** | 高（两层） | 低（一层） |

---

## ⚠️ 注意事项

### 1. IService 不是必需的

**两种服务类型：**

**A. IService 服务（推荐）**
```cpp
// 自动管理生命周期
app.registerService<HttpServerService>(8080);
// Application 自动调用 initialize()/start()/stop()
```

**B. 普通服务（灵活）**
```cpp
// 手动管理生命周期
app.registerService<VideoPipeline>("pipeline", ...);

app.onStart([&app]() {
    auto pipeline = app.getService<VideoPipeline>("pipeline");
    return pipeline->start();
});
```

**选择建议：**
- ✅ 需要自动生命周期管理 → 实现 `IService`
- ✅ 需要灵活的初始化逻辑 → 使用回调

---

### 2. 服务注册顺序

```cpp
// ✅ 推荐：先注册底层服务，再注册依赖服务
app.registerService<DatabaseService>();
app.registerService<CacheService>();
app.registerService<HttpServerService>();  // 依赖 Database 和 Cache
```

**停止顺序：** 自动逆序（保证依赖关系）
```
启动：Database → Cache → HttpServer
停止：HttpServer → Cache → Database  ← 逆序
```

---

### 3. 避免循环依赖

```cpp
// ❌ 错误：A 依赖 B，B 又依赖 A
class AService : public IService {
    bool initialize() override {
        auto b = Application::getInstance().getService<BService>();
        b->doSomething();  // B 可能还没初始化
        return true;
    }
};

// ✅ 正确：单向依赖
class BService : public IService {
    bool initialize() override {
        // B 不依赖 A
        return true;
    }
};

class AService : public IService {
    bool start() override {  // 在 start 阶段访问，而非 initialize
        auto b = Application::getInstance().getService<BService>();
        b->doSomething();
        return true;
    }
};
```

---

### 4. CMake 配置

**应用程序需要链接的库：**
```cmake
target_link_libraries(my_app
    PRIVATE
        common_lib      # Application + IService 管理
        web_lib         # HttpServerService 等
        net_lib         # HttpClientPoolService 等
        videopipeline_lib
        log_lib
        ...
)
```

**注意：**
- ✅ `common_lib` **不依赖** `web_lib`（使用前向声明）
- ✅ 应用程序同时链接两者
- ✅ 无循环依赖

---

## 🎯 最佳实践

### 1. 为新模块实现 IService

```cpp
// my_module/include/my_module/my_service.h
#pragma once
#include "web/service/iservice.h"

class MyService : public IService {
public:
    bool initialize() override;
    bool start() override;
    void stop() override;
    const char* getName() const override { return "MyService"; }
    bool isRunning() const override { return running_; }
    bool isInitialized() const override { return initialized_; }
    
private:
    bool initialized_ = false;
    bool running_ = false;
};
```

### 2. 在 Application 中注册

```cpp
auto& app = Application::getInstance();
app.registerService<MyService>(args...);
```

### 3. 享受自动生命周期管理

```cpp
// Application 自动调用：
// 1. MyService::initialize()
// 2. MyService::start()
// 3. 等待信号
// 4. MyService::stop()
return app.run();
```

---

## 📚 相关文档

- [IService 接口定义](../modules/web/include/web/service/iservice.h)
- [Application Framework Guide](../APPLICATION_FRAMEWORK_GUIDE.md)
- [Common Module README](../modules/common/README.md)

---

## 🎉 总结

**新设计的优势：**

1. ✅ **保留 IService** - 优秀的统一接口标准
2. ✅ **简化架构** - 移除 ServiceContainer，减少层级
3. ✅ **统一管理** - Application 直接管理所有 IService
4. ✅ **自动化** - 自动生命周期管理 + 信号处理
5. ✅ **灵活性** - 支持 IService 和普通服务混合使用

**推荐使用方式：**
- 新模块 → 实现 `IService` 接口
- 注册到 `Application`
- 享受自动生命周期管理

这样既保留了 `IService` 的优秀设计，又简化了整体架构！🚀
