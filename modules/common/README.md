# Common Module - 通用模块

## 📋 概述

提供跨平台的通用功能组件，包括信号处理和应用框架。

## 🏗️ 目录结构

```
modules/common/
├── CMakeLists.txt              # 模块构建配置
├── include/
│   └── common/
│       ├── signal_handler.h    # 跨平台信号处理器
│       └── application.h       # 应用程序框架
├── src/
│   ├── signal_handler.cpp      # 信号处理实现
│   └── application.cpp         # 应用框架实现
└── test/                       # 测试文件（可选）
```

## 🎯 核心组件

### 1. SignalHandler - 跨平台信号处理器

**功能：**
- ✅ Windows/Linux/macOS 统一接口
- ✅ 线程安全的信号回调
- ✅ 阻塞等待或轮询检查
- ✅ 优雅关闭支持

**使用示例：**
```cpp
#include "common/signal_handler.h"

SignalHandler handler;
handler.initialize();

// 注册回调
handler.registerCallback(SignalHandler::SIGINT_VAL, [](int signum) {
    std::cout << "Received signal, shutting down..." << std::endl;
});

// 等待信号
handler.waitForSignal();
```

### 2. Application - 应用程序框架

**功能：**
- ✅ 依赖注入容器
- ✅ 配置管理
- ✅ 生命周期管理（Init → Start → Run → Stop）
- ✅ 信号处理集成
- ✅ 日志系统初始化

**使用示例：**
```cpp
#include "common/application.h"

int main() {
    auto& app = Application::getInstance();
    
    // 初始化
    app.initLogger("logs", "debug");
    app.loadConfig("config.yaml");
    
    // 注册服务
    app.registerService<MyService>("service", args...);
    
    // 生命周期回调
    app.onInit([]() { /* 初始化 */ return true; });
    app.onStart([]() { /* 启动 */ return true; });
    app.onStop([]() { /* 清理 */ });
    
    // 运行（阻塞直到 Ctrl+C）
    return app.run();
}
```

## 🔧 编译配置

### 依赖项

- `log_lib` - 日志模块（必需）

### CMake 集成

```cmake
# 在主 CMakeLists.txt 中
add_subdirectory(modules/common)

# 链接到其他目标
target_link_libraries(your_target PRIVATE common_lib)
```

## 📖 详细文档

完整的使用指南请参考：[APPLICATION_FRAMEWORK_GUIDE.md](../../APPLICATION_FRAMEWORK_GUIDE.md)

## 💡 典型应用场景

1. **视频流处理应用**
   ```cpp
   app.registerService<VideoPipeline>("pipeline", io_ctx, config);
   app.onStart([&app]() {
       auto pipeline = app.getService<VideoPipeline>("pipeline");
       return pipeline->start();
   });
   ```

2. **网络服务器**
   ```cpp
   app.registerService<HttpServer>("server", port);
   app.onStop([&app]() {
       auto server = app.getService<HttpServer>("server");
       server->stop();
   });
   ```

3. **后台服务**
   ```cpp
   app.onInit([&app]() {
       // 初始化数据库、缓存等
       return true;
   });
   
   app.run();  // 持续运行直到收到信号
   ```

## ⚠️ 注意事项

1. **Application 是单例**
   - 整个程序只能有一个 Application 实例
   - 使用 `Application::getInstance()` 获取

2. **信号处理限制**
   - 信号处理器中只能调用异步安全的函数
   - 复杂逻辑应在主线程的回调中执行

3. **线程安全**
   - SignalHandler 和 Application 的服务容器是线程安全的
   - 服务本身的线程安全性由服务自己保证

4. **优雅关闭超时**
   - 默认最多等待 10 秒完成清理
   - 超时后强制退出

## 🚀 快速开始

查看完整示例：[apps/app_with_framework.cpp](../../apps/app_with_framework.cpp)

```bash
# 编译
cmake --build out/build/x64-Debug --target app_with_framework

# 运行
cd bin
./app_with_framework.exe

# 按 Ctrl+C 优雅退出
```

## 📝 更新日志

### v1.0.0 (2026-04-15)
- ✅ 初始版本
- ✅ 跨平台信号处理器
- ✅ 应用程序框架
- ✅ 依赖注入容器
- ✅ 生命周期管理
