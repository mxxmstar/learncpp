# Service 架构使用指南

## 📁 目录结构

```
include/
├── service/
│   ├── iservice.h              # 服务接口基类
│   ├── service_container.h     # 服务容器（单例）
│   ├── http_server_service.h   # HTTP 服务器服务
│   └── zlm_service.h           # ZLMediaKit 服务
src/
├── service/
│   ├── http_server_service.cpp
│   └── zlm_service.cpp
├── main_new.cpp                # 新的主程序入口（示例）
```

## 🏗️ 架构设计

### 分层结构
```
应用层 (main.cpp)
    ↓
服务容器层 (ServiceContainer)
    ↓
服务层 (HttpServerService, ZLMService, ...)
    ↓
核心模块层 (AsioHttpServer, ZLMManager, ...)
    ↓
基础设施层 (ConfigManager, LogManager)
```

## ✅ 已实现的功能

### 1. IService 接口
所有服务的基类，定义了统一的生命周期方法：
- `initialize()` - 初始化（只进行一次）
- `start()` - 启动
- `stop()` - 停止
- `getName()` - 获取服务名称
- `isRunning()` - 是否正在运行
- `isInitialized()` - 是否已初始化

### 2. ServiceContainer 服务容器
单例模式，负责：
- 注册服务
- 获取服务
- 统一初始化所有服务
- 统一启动所有服务
- 统一停止所有服务（逆序停止）

### 3. HttpServerService
封装了原有的 `AsioHttpServer`，提供：
- HTTP 服务
- 可访问底层的 `io_context` 和 `AsioHttpServer`

### 4. ZLMService
封装了原有的 `ZLMManager`，提供：
- ZLMediaKit 进程管理
- 流媒体服务

## 🚀 使用方法

### 步骤 1：创建服务

```cpp
// 创建 HTTP 服务器服务
auto http_svc = std::make_shared<HttpServerService>(config.server);

// 创建 ZLM 服务
auto zlm_svc = std::make_shared<ZLMService>(shared_ctx, config.media);
```

### 步骤 2：注册到容器

```cpp
auto& container = ServiceContainer::getInstance();

container.registerService<HttpServerService>(config.server);
container.registerService<ZLMService>(shared_ctx, config.media);
```

### 步骤 3：初始化和启动

```cpp
// 初始化所有服务
if (!container.initializeAll()) {
    LOG_MAIN_ERROR_AT("Failed to initialize services");
    return 1;
}

// 启动所有服务
if (!container.startAll()) {
    LOG_MAIN_ERROR_AT("Failed to start services");
    return 1;
}
```

### 步骤 4：运行

```cpp
// 运行 HTTP 服务器的 io_context
http_svc->getIoContext()->run();
```

### 步骤 5：停止和清理

```cpp
// 停止所有服务（自动按逆序停止）
container.stopAll();

// 清理日志
log_mgr.Shutdown();
```

## 📝 如何添加新服务

### 1. 创建服务头文件

```cpp
// include/service/xxx_service.h
#pragma once

#include "service/iservice.h"
#include "config/common_config.h"

class XxxService : public IService {
public:
    explicit XxxService(const XxxConfig& config);
    ~XxxService() override;
    
    bool initialize() override;
    bool start() override;
    void stop() override;
    const char* getName() const override { return "XxxService"; }
    bool isRunning() const override { return running_; }
    bool isInitialized() const override { return initialized_; }
    
private:
    XxxConfig config_;
    bool initialized_ = false;
    bool running_ = false;
};
```

### 2. 创建服务实现文件

```cpp
// src/service/xxx_service.cpp
#include "service/xxx_service.h"
#include "log/logmanager.h"

XxxService::XxxService(const XxxConfig& config)
    : config_(config) {
}

XxxService::~XxxService() {
    if (running_) {
        stop();
    }
}

bool XxxService::initialize() {
    if (initialized_) return true;
    
    LOG_MAIN_INFO_AT("{}: Initializing...", getName());
    // 初始化逻辑
    
    initialized_ = true;
    return true;
}

bool XxxService::start() {
    if (!initialized_) return false;
    if (running_) return true;
    
    LOG_MAIN_INFO_AT("{}: Starting...", getName());
    // 启动逻辑
    
    running_ = true;
    return true;
}

void XxxService::stop() {
    if (!running_) return;
    
    LOG_MAIN_INFO_AT("{}: Stopping...", getName());
    // 停止逻辑
    
    running_ = false;
}
```

### 3. 在 main.cpp 中注册和使用

```cpp
#include "service/xxx_service.h"

int main() {
    // ... 配置和日志初始化 ...
    
    auto& container = ServiceContainer::getInstance();
    
    // 注册新服务
    container.registerService<XxxService>(config.xxx);
    
    // 初始化和启动
    container.initializeAll();
    container.startAll();
    
    // ... 运行 ...
    
    // 停止
    container.stopAll();
}
```

## 🎯 优势

### 1. 不修改原有代码
- 所有 Service 都是新增的
- 原有模块（HTTPServer、ZLMManager 等）保持不变
- 随时可以回退到旧版本

### 2. 统一的生命周期管理
- 所有服务都有 initialize/start/stop
- 统一的错误处理
- 统一的日志输出

### 3. 依赖注入
- 通过构造函数传递依赖
- 避免了全局单例的滥用
- 易于测试和维护

### 4. 易于扩展
- 新增服务只需实现 IService 接口
- 自动管理启动/停止顺序
- 支持动态获取服务

### 5. 优雅退出
- 信号处理
- 逆序停止服务
- 完整的清理流程

## ⚠️ 注意事项

### 1. ConfigManager 和 LogManager 仍然是单例
这是合理的，因为它们是基础设施，应该全局唯一。

### 2. ServiceContainer 也是单例
但这是必要的单例，用于统一管理所有服务。

### 3. 服务之间的依赖关系
如果服务 A 依赖服务 B，确保：
- B 在 A 之前注册（先启动）
- A 在 B 之后停止（先停止依赖）

### 4. io_context 的管理
- HTTP Server 有自己的 io_context
- ZLM 等服务可以共享一个 io_context
- 根据需要决定是否需要多个 io_context

## 📋 下一步计划

可以考虑添加的服务：
- WebSocketService - WebSocket 服务
- CameraService - 摄像头管理服务
- DatabaseService - 数据库服务
- FFmpegService - FFmpeg 转码服务

每个服务都遵循相同的模式，易于理解和维护。
