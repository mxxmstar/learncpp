# Application Framework 使用指南

## 📋 目录
1. [概述](#概述)
2. [架构设计](#架构设计)
3. [快速开始](#快速开始)
4. [核心功能](#核心功能)
5. [最佳实践](#最佳实践)

---

## 概述

Application Framework 是一个跨平台的应用程序框架，提供：

- ✅ **依赖注入容器** - 管理服务生命周期
- ✅ **配置管理** - 加载和访问配置
- ✅ **信号处理** - 跨平台的优雅关闭
- ✅ **生命周期管理** - Init → Start → Run → Stop
- ✅ **日志集成** - 自动初始化日志系统

### 适用场景

- 视频流处理应用
- 网络服务器
- 后台服务
- 需要优雅关闭的长时间运行程序

---

## 架构设计

### 组件关系

```
┌─────────────────────────────────────┐
│         Application (Singleton)     │
├─────────────────────────────────────┤
│  ┌──────────────┐  ┌─────────────┐ │
│  │SignalHandler │  │   Config    │ │
│  └──────────────┘  └─────────────┘ │
│  ┌──────────────┐  ┌─────────────┐ │
│  │Service Container│ │  Logger   │ │
│  └──────────────┘  └─────────────┘ │
│  ┌───────────────────────────────┐ │
│  │  Lifecycle Callbacks          │ │
│  │  - onInit()                   │ │
│  │  - onStart()                  │ │
│  │  - onStop()                   │ │
│  └───────────────────────────────┘ │
└─────────────────────────────────────┘
```

### 生命周期

```
创建 → 初始化 → 启动 → 运行 → 停止 → 销毁
        ↓        ↓              ↓
      onInit   onStart       onStop
```

---

## 快速开始

### 1. 最简单的应用

```cpp
#include "common/application.h"

int main() {
    auto& app = Application::getInstance();
    
    // 初始化日志
    app.initLogger("logs", "info");
    
    // 注册回调
    app.onStart([]() {
        std::cout << "Application started!" << std::endl;
        return true;
    });
    
    app.onStop([]() {
        std::cout << "Application stopped!" << std::endl;
    });
    
    // 运行（阻塞直到 Ctrl+C）
    return app.run();
}
```

### 2. 完整示例

参考 `apps/app_with_framework.cpp`

---

## 核心功能

### 1. 依赖注入容器

#### 注册服务

```cpp
// 注册单例服务
auto io_ctx = std::make_shared<boost::asio::io_context>();
app.registerService<boost::asio::io_context>("io_context", *io_ctx);

// 注册复杂对象
PipelineConfig config;
config.channel_id = 1;
config.stream_url = "http://...";

app.registerService<VideoPipeline>("pipeline", io_ctx, config);
```

#### 获取服务

```cpp
// 类型安全地获取服务
auto pipeline = app.getService<VideoPipeline>("pipeline");
if (pipeline) {
    pipeline->start();
}

// 检查服务是否存在
if (app.hasService("pipeline")) {
    // 服务已注册
}
```

#### 服务生命周期

```
registerService() → 创建并存储
       ↓
getService()    → 获取共享指针
       ↓
Application 销毁 → 自动清理所有服务
```

---

### 2. 配置管理

#### 加载配置文件

```cpp
// 从 YAML 文件加载（TODO: 实现完整解析）
app.loadConfig("tools/config.yaml");
```

#### 访问配置

```cpp
// 获取配置项（带默认值）
std::string log_dir = app.getConfig<std::string>("log.dir", "logs");
int port = app.getConfig<int>("server.port", 8080);
bool debug = app.getConfig<bool>("debug", false);

// 设置配置项
app.setConfig("custom.key", "value");
```

#### 配置优先级

```
1. 配置文件 (YAML/JSON)
2. 命令行参数 (TODO)
3. 环境变量 (TODO)
4. 默认值
```

---

### 3. 信号处理

#### 跨平台支持

| 信号 | Windows | Linux/macOS | 说明 |
|------|---------|-------------|------|
| SIGINT | ✅ Ctrl+C | ✅ Ctrl+C | 中断 |
| SIGTERM | ✅ | ✅ | 终止 |
| SIGBREAK | ✅ Ctrl+Break | ❌ | Windows 特有 |
| SIGHUP | ❌ | ✅ | 重新加载配置 |
| SIGUSR1 | ❌ | ✅ | 用户自定义 1 |
| SIGUSR2 | ❌ | ✅ | 用户自定义 2 |

#### 注册信号回调

```cpp
auto& handler = app.getSignalHandler();

// 注册 SIGINT 处理
handler.registerCallback(SignalHandler::SIGINT_VAL, [](int signum) {
    std::cout << "Received SIGINT, shutting down..." << std::endl;
    // 执行清理操作
});

// 注册 SIGHUP（重新加载配置）
#ifdef __linux__
handler.registerCallback(SignalHandler::SIGHUP_VAL, [](int signum) {
    std::cout << "Received SIGHUP, reloading config..." << std::endl;
    // 重新加载配置
});
#endif
```

#### 等待信号

```cpp
// 阻塞直到收到信号
handler.waitForSignal();

// 或者轮询检查
while (!handler.shouldStop()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

---

### 4. 生命周期管理

#### 初始化阶段 (onInit)

```cpp
app.onInit([&app]() {
    std::cout << "Initializing..." << std::endl;
    
    // 1. 获取服务
    auto pipeline = app.getService<VideoPipeline>("pipeline");
    
    // 2. 配置服务
    pipeline->setFrameOutputCallback(...);
    
    // 3. 返回 false 表示初始化失败
    return true;
});
```

#### 启动阶段 (onStart)

```cpp
app.onStart([&app]() {
    std::cout << "Starting..." << std::endl;
    
    // 1. 启动服务
    auto pipeline = app.getService<VideoPipeline>("pipeline");
    if (!pipeline->start()) {
        return false;  // 启动失败
    }
    
    // 2. 启动后台线程
    std::thread worker([]() {
        // 工作线程逻辑
    });
    
    return true;
});
```

#### 停止阶段 (onStop)

```cpp
app.onStop([&app]() {
    std::cout << "Stopping..." << std::endl;
    
    // 1. 停止服务（逆序执行）
    auto pipeline = app.getService<VideoPipeline>("pipeline");
    if (pipeline) {
        pipeline->stop();
    }
    
    // 2. 停止 io_context
    auto io_ctx = app.getService<boost::asio::io_context>("io_context");
    if (io_ctx) {
        io_ctx->stop();
    }
    
    // 3. 清理资源
    // ...
});
```

#### 运行主循环

```cpp
// 方式 1：使用 Application 的主循环（推荐）
int exit_code = app.run();

// 方式 2：自定义主循环
app.onStart([&app]() {
    // 启动服务
    return true;
});

// 在后台线程运行 io_context
std::thread io_thread([&io_ctx]() {
    io_ctx->run();
});

// 等待停止信号
while (app.isRunning() && !app.getSignalHandler().shouldStop()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 打印统计信息
    std::cout << "FPS: " << calculate_fps() << std::endl;
}

// 请求停止
app.requestStop();

// 等待线程结束
io_thread.join();
```

---

### 5. 日志系统集成

#### 自动初始化

```cpp
// 一行代码初始化日志
app.initLogger("logs", "debug");

// 等价于：
// LogManager::getInstance().Init();
```

#### 日志输出

```cpp
#include "log/logmanager.h"

LOG_MAIN_INFO_AT("Application started");
LOG_MAIN_DEBUG_AT("Processing frame #{}", frame_count);
LOG_MAIN_ERROR_AT("Failed to connect: {}", error_message);
```

---

## 最佳实践

### 1. 服务注册顺序

```cpp
// ✅ 推荐：先注册基础服务，再注册依赖服务
app.registerService<boost::asio::io_context>("io_context", io_ctx);
app.registerService<VideoPipeline>("pipeline", io_ctx, config);
app.registerService<AlgorithmProcessor>("algo", pipeline);

// ❌ 避免：循环依赖
// A 依赖 B，B 又依赖 A
```

### 2. 错误处理

```cpp
app.onInit([&app]() {
    try {
        auto service = app.getService<MyService>("service");
        if (!service) {
            LOG_MAIN_ERROR_AT("Service not found");
            return false;
        }
        
        if (!service->initialize()) {
            LOG_MAIN_ERROR_AT("Service initialization failed");
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Exception: {}", e.what());
        return false;
    }
});
```

### 3. 优雅关闭

```cpp
// ✅ 推荐：给清理操作设置超时
app.onStop([&app]() {
    auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(10);
    
    // 停止服务
    auto pipeline = app.getService<VideoPipeline>("pipeline");
    if (pipeline) {
        pipeline->stop();
    }
    
    // 检查是否超时
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed > timeout) {
        LOG_MAIN_WARN_AT("Shutdown timeout, forcing exit");
    }
});
```

### 4. 多线程安全

```cpp
// ✅ SignalHandler 是线程安全的
app.getSignalHandler().registerCallback(...);

// ✅ Application 的服务容器是线程安全的（读操作）
auto service = app.getService<MyService>("name");

// ⚠️ 注意：服务本身的线程安全性由服务自己保证
```

### 5. 配置热重载

```cpp
// 注册 SIGHUP 处理（Linux/macOS）
#ifdef __linux__
app.getSignalHandler().registerCallback(
    SignalHandler::SIGHUP_VAL, 
    [&app](int signum) {
        std::cout << "Reloading configuration..." << std::endl;
        app.loadConfig("config.yaml");
        
        // 通知服务重新加载配置
        auto service = app.getService<MyService>("service");
        if (service) {
            service->reloadConfig();
        }
    }
);
#endif
```

---

## 编译和运行

### 编译

```bash
# CMake 会自动包含 app_with_framework（如果 BUILD_TESTS=ON）
cmake --build out/build/x64-Debug --target app_with_framework
```

### 运行

```bash
cd bin
./app_with_framework.exe

# 按 Ctrl+C 优雅退出
```

### 预期输出

```
========================================
  Application Starting
========================================

[SignalHandler] Initialized successfully
[Application] Loading config from: tools/config.yaml
[Application] Logger initialized: dir=logs, level=debug
[Application] Initializing...
[Main] Initializing video pipeline...
[Application] Initialized successfully
[Application] Starting...
[Main] Starting video pipeline...
[IO Thread] Running io_context...
[Application] Started successfully

[Application] Running... (Press Ctrl+C to stop)
[Main] Processed 30 frames
[Main] Processed 60 frames
^C
[SignalHandler] Received SIGINT (Ctrl+C)
[Application] Stop requested

[Application] Graceful shutdown initiated...
[Main] Stopping video pipeline...
[Application] Stopping...
[Main] All services stopped
[Application] Stopped
[Application] Shutdown completed in 150ms

========================================
  Application Stopped
========================================
```

---

## 常见问题

### Q1: 为什么使用单例模式？

**A**: Application 需要全局访问点，特别是在信号处理器中。单例确保只有一个实例，避免状态不一致。

### Q2: 如何测试 Application？

**A**: 使用依赖注入，可以轻松 mock 服务：

```cpp
// 测试代码
TEST(ApplicationTest, ServiceInjection) {
    auto& app = Application::getInstance();
    
    // 注册 mock 服务
    auto mock_service = std::make_shared<MockService>();
    app.registerService<IService>("service", mock_service);
    
    // 验证服务可获取
    auto service = app.getService<IService>("service");
    ASSERT_NE(service, nullptr);
}
```

### Q3: 如何处理多个 VideoPipeline？

**A**: 为每个管道注册不同的服务名：

```cpp
app.registerService<VideoPipeline>("pipeline_cam1", io_ctx, config1);
app.registerService<VideoPipeline>("pipeline_cam2", io_ctx, config2);
app.registerService<VideoPipeline>("pipeline_cam3", io_ctx, config3);

// 使用时
auto cam1 = app.getService<VideoPipeline>("pipeline_cam1");
auto cam2 = app.getService<VideoPipeline>("pipeline_cam2");
```

### Q4: 如何在 Windows 下调试信号处理？

**A**: Windows 控制台程序的信号处理与 Linux 不同：
- 使用 `Ctrl+C` 触发 SIGINT
- 使用 `Ctrl+Break` 触发 SIGBREAK
- 在 Visual Studio 中，可以在 `platformSignalHandler` 设置断点

---

## 总结

Application Framework 提供了：

1. ✅ **统一的应用结构** - 所有应用遵循相同模式
2. ✅ **优雅的资源管理** - 自动清理，避免泄漏
3. ✅ **跨平台兼容** - Windows/Linux/macOS
4. ✅ **易于测试** - 依赖注入方便 mock
5. ✅ **可扩展** - 通过回调轻松扩展功能

**推荐在所有新应用中使用此框架！** 🎉
