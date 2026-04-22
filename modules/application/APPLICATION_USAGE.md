# Application 模块使用文档

## 📋 概述

Application 模块是一个轻量级的应用程序框架，提供以下核心功能：

- ✅ **依赖注入容器** - 管理服务生命周期
- ✅ **生命周期管理** - 初始化、启动、停止的完整流程
- ✅ **信号处理** - 优雅关闭支持
- ✅ **IService 集成** - 自动管理服务生命周期

> **注意**：配置管理由 `ConfigManager` 模块负责，Application 不重复实现配置加载功能。
> 使用配置时，请直接调用 `ConfigManager::GetInstance().GetConfig()`。

---

## 🚀 快速开始

### 基本用法

```cpp
#include "application/application.h"
#include "log/logmanager.h"

int main() {
    // 1. 获取 Application 单例
    auto& app = Application::GetInstance();
    
    // 2. 加载配置（可选）
    app.LoadConfig("config.yaml");
    
    // 3. 注册服务
    app.RegisterService<MyService>("my_service");
    
    // 4. 注册生命周期回调
    app.OnInit([]() {
        LOG_MAIN_INFO_AT("Initializing...");
        return true;
    });
    
    app.OnStart([]() {
        LOG_MAIN_INFO_AT("Starting...");
        return true;
    });
    
    app.OnStop([]() {
        LOG_MAIN_INFO_AT("Stopping...");
    });
    
    // 5. 运行应用（阻塞直到收到停止信号）
    return app.Run();
}
```

---

## 📦 核心功能

### 1. 依赖注入容器

```cpp
// 方式 1：按名称注册（模板参数推导）
app.RegisterService<HttpServer>("http_server", config);
app.RegisterService<Database>("database", connection_string);

// 方式 2：注册 IService 实现（自动管理生命周期）
app.RegisterService<ZLMService>();
app.RegisterService<HttpClientPoolService>();
```

#### 获取服务

```cpp
// 方式 1：按名称获取
auto http_server = app.GetService<HttpServer>("http_server");
if (http_server) {
    http_server->start();
}

// 方式 2：获取 IService 实现
auto zlm_service = app.GetService<ZLMService>();
if (zlm_service) {
    zlm_service->doSomething();
}

// 方式 3：通过名称获取 IService
auto service = app.GetService("zlm_service");
```

#### 检查服务是否存在

```cpp
if (app.HasService("http_server")) {
    LOG_MAIN_INFO_AT("HTTP server is registered");
}
```

---

### 2. 生命周期管理

Application 提供了三个生命周期阶段：

#### 初始化阶段（OnInit）

在应用启动时调用，用于：
- 初始化资源
- 验证配置
- 准备环境

```cpp
app.OnInit([]() {
    LOG_MAIN_INFO_AT("Initializing database connection...");
    
    auto db = app.GetService<Database>("database");
    if (!db->connect()) {
        LOG_MAIN_ERROR_AT("Failed to connect to database");
        return false;  // 返回 false 会终止启动
    }
    
    return true;  // 返回 true 继续启动
});
```

#### 启动阶段（OnStart）

在初始化成功后调用，用于：
- 启动服务
- 开始监听
- 启动后台任务

```cpp
app.OnStart([]() {
    LOG_MAIN_INFO_AT("Starting HTTP server...");
    
    auto server = app.GetService<HttpServer>("http_server");
    if (!server->start()) {
        LOG_MAIN_ERROR_AT("Failed to start HTTP server");
        return false;
    }
    
    return true;
});
```

#### 停止阶段（OnStop）

在应用停止时调用，用于：
- 清理资源
- 保存状态
- 关闭连接

```cpp
app.OnStop([]() {
    LOG_MAIN_INFO_AT("Saving application state...");
    
    auto db = app.GetService<Database>("database");
    db->disconnect();
    
    LOG_MAIN_INFO_AT("Cleanup completed");
});
```

---

### 3. 信号处理

Application 内置了信号处理器，支持优雅关闭：

```cpp
// 获取信号处理器
auto& signal_handler = app.GetSignalHandler();

// 注册自定义信号回调
signal_handler.registerCallback(SignalHandler::SIGINT_VAL, [](int signum) {
    LOG_MAIN_INFO_AT("Received signal: {}", SignalHandler::getSignalName(signum));
});

// 检查是否应该停止
if (signal_handler.shouldStop()) {
    // 执行清理
}
```

**支持的信号**：
- `SIGINT` (Ctrl+C)
- `SIGTERM` (终止信号)

---

### 4. IService 管理

Application 可以自动管理实现了 `IService` 接口的服务：

#### IService 接口

```cpp
class IService {
public:
    virtual ~IService() = default;
    
    virtual std::string GetName() const = 0;
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isInitialized() const = 0;
    virtual bool isRunning() const = 0;
};
```

#### 注册 IService

```cpp
// 自动调用 initialize() 和 start()
app.RegisterService<ZLMService>();
app.RegisterService<HttpClientPoolService>();
```

#### 生命周期自动管理

Application 会按照以下顺序管理服务：

1. **初始化阶段**：按注册顺序调用 `initialize()`
2. **启动阶段**：按注册顺序调用 `start()`
3. **停止阶段**：按注册逆序调用 `stop()`

---

## 💡 完整示例

```cpp
#include "application/application.h"
#include "config/common_config.h"
#include "log/logmanager.h"
#include "web/service/http_server_service.h"
#include "zlmediakit/service/zlm_service.h"

int main() {
    try {
        // === 1. 初始化日志 ===
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init("./logs", 1);
        
        LOG_MAIN_INFO_AT("Application starting...");
        
        // === 2. 加载配置（使用 ConfigManager）===
        ConfigManager& config_mgr = ConfigManager::GetInstance();
        if (!config_mgr.Load("config.yaml")) {
            LOG_MAIN_WARN_AT("Config not found, using defaults");
        }
        
        // === 3. 重新配置日志 ===
        const auto& config = config_mgr.GetConfig();
        if (!config.logs.empty()) {
            log_mgr.ReloadFromConfigs(config.logs);
        }
        
        // === 4. 获取 Application 实例 ===
        auto& app = Application::GetInstance();
        
        // === 5. 注册 IService 服务 ===
        app.RegisterService<ZLMService>();
        app.RegisterService<HttpClientPoolService>();
        
        // === 6. 注册生命周期回调 ===
        
        // 初始化阶段
        app.OnInit([&app]() {
            LOG_MAIN_INFO_AT("Initializing services...");
            
            auto zlm = app.GetService<ZLMService>();
            if (!zlm) {
                LOG_MAIN_ERROR_AT("Failed to get ZLM service");
                return false;
            }
            
            return true;
        });
        
        // 启动阶段
        app.OnStart([&app]() {
            LOG_MAIN_INFO_AT("Starting services...");
            
            auto zlm = app.GetService<ZLMService>();
            if (zlm && !zlm->isRunning()) {
                if (!zlm->start()) {
                    LOG_MAIN_ERROR_AT("Failed to start ZLM service");
                    return false;
                }
            }
            
            return true;
        });
        
        // 停止阶段
        app.OnStop([&app]() {
            LOG_MAIN_INFO_AT("Stopping services...");
            
            auto zlm = app.GetService<ZLMService>();
            if (zlm && zlm->isRunning()) {
                zlm->stop();
            }
        });
        
        // === 7. 运行应用 ===
        LOG_MAIN_INFO_AT("Application running... (Press Ctrl+C to stop)");
        int exit_code = app.Run();
        
        LOG_MAIN_INFO_AT("Application exited with code: {}", exit_code);
        return exit_code;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Fatal error: {}", e.what());
        return 1;
    }
}
```

---

## 📊 API 参考

### Application 类

#### 静态方法

| 方法 | 说明 |
|------|------|
| `static Application& GetInstance()` | 获取单例实例 |

#### 依赖注入

| 方法 | 说明 |
|------|------|
| `void RegisterService<T>(name, args...)` | 注册服务（按名称） |
| `bool RegisterService<T>(args...)` | 注册 IService 服务 |
| `std::shared_ptr<T> GetService<T>(name)` | 获取服务（按名称） |
| `T* GetService<T>()` | 获取 IService 服务 |
| `std::shared_ptr<IService> GetService(name)` | 通过名称获取 IService |
| `bool HasService(name)` | 检查服务是否存在 |

#### 生命周期管理

| 方法 | 说明 |
|------|------|
| `void OnInit(callback)` | 注册初始化回调 |
| `void OnStart(callback)` | 注册启动回调 |
| `void OnStop(callback)` | 注册停止回调 |
| `int Run()` | 运行应用（阻塞） |
| `void RequestStop()` | 请求停止 |
| `bool IsRunning()` | 检查是否正在运行 |

#### 信号处理

| 方法 | 说明 |
|------|------|
| `SignalHandler& GetSignalHandler()` | 获取信号处理器 |

---

## ⚠️ 注意事项

### 1. 线程安全

- ✅ `RegisterService`、`GetService` 是线程安全的
- ✅ `Run()` 会在主线程中阻塞
- ⚠️ 回调函数中避免长时间阻塞操作

### 2. 服务注册顺序

- IService 服务会按注册顺序初始化和启动
- 停止时会按逆序执行
- 确保依赖关系正确的注册顺序

### 3. 回调返回值

- `OnInit` 和 `OnStart` 回调返回 `false` 会终止启动
- `OnStop` 回调没有返回值

### 4. 优雅关闭

- 收到 SIGINT/SIGTERM 信号会自动触发停止
- 调用 `RequestStop()` 也会触发停止
- 最多等待 10 秒完成清理

---

## 🔗 相关文档

- [IService 接口文档](../common/ISERVICE.md)
- [LogManager 使用文档](../log/LOGMANAGER_USAGE.md)
- [ConfigManager 使用文档](../config/CONFIG_MANAGER_USAGE.md)

---

## 📝 更新日志

### v1.0.0 (2026-04-21)

- ✅ 初始版本
- ✅ 依赖注入容器
- ✅ 配置管理
- ✅ 生命周期管理
- ✅ 信号处理
- ✅ IService 集成
- ✅ 所有 public 方法改为大驼峰命名
