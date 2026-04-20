# LogManager FlushAll 接口说明

## 🎯 功能说明

`LogManager::FlushAll()` 用于立即刷新所有 logger 的异步缓冲区，确保所有待输出的日志立即写入文件或控制台。

---

## 💡 使用场景

### 1. **调试时确保日志完整输出**

```cpp
int main() {
    LOG_MAIN_INFO_AT("Application starting...");
    
    // 做一些操作
    doSomething();
    
    // 确保所有日志都输出了
    LogManager::getInstance().FlushAll();
    
    return 0;
}
```

---

### 2. **程序异常退出前刷新日志**

```cpp
try {
    runApp();
} catch (const std::exception& e) {
    LOG_MAIN_ERROR_AT("Fatal error: {}", e.what());
    
    // 确保错误日志被写入
    LogManager::getInstance().FlushAll();
    
    return 1;
}
```

---

### 3. **关键操作后立即刷新**

```cpp
void criticalOperation() {
    LOG_MAIN_INFO_AT("Starting critical operation...");
    
    // 执行关键操作
    performCriticalTask();
    
    LOG_MAIN_INFO_AT("Critical operation completed");
    
    // 立即刷新，确保日志不丢失
    LogManager::getInstance().FlushAll();
}
```

---

### 4. **测试用例中验证日志输出**

```cpp
TEST_F(LogTest, TestAsyncLogging) {
    LOG_MAIN_INFO_AT("Test message 1");
    LOG_MAIN_INFO_AT("Test message 2");
    
    // 刷新缓冲区
    LogManager::getInstance().FlushAll();
    
    // 现在可以检查日志文件内容
    EXPECT_TRUE(logFileContains("Test message 1"));
    EXPECT_TRUE(logFileContains("Test message 2"));
}
```

---

## 🔧 API 说明

### 函数签名

```cpp
void LogManager::FlushAll();
```

### 功能

1. ✅ 遍历所有注册的 logger
2. ✅ 调用每个 logger 的 `Flush()` 方法
3. ✅ 刷新 spdlog 的全局异步缓冲区
4. ✅ 线程安全（使用 mutex 保护）

---

## 📊 工作原理

### 异步日志流程

```
应用程序
   │
   ├─→ LOG_MAIN_INFO_AT("message")
   │      └─→ 消息进入异步队列
   │
   ├─→ 后台线程从队列取消息
   │      └─→ 写入文件/控制台（延迟）
   │
   └─→ FlushAll()  ← 强制立即刷新
          └─→ 清空队列，立即写入
```

---

### 为什么需要 FlushAll？

**问题**：
- spdlog 默认使用异步模式
- 日志消息先进入队列
- 后台线程异步写入
- 程序退出时可能还有未写入的消息

**解决**：
- `FlushAll()` 强制刷新所有缓冲区
- 确保所有日志都已被写入
- 避免日志丢失

---

## ⚠️ 注意事项

### 1. **性能影响**

```cpp
// ❌ 错误：频繁调用会影响性能
for (int i = 0; i < 1000; ++i) {
    LOG_MAIN_INFO_AT("Message {}", i);
    LogManager::getInstance().FlushAll();  // 每次循环都刷新，很慢！
}

// ✅ 正确：只在必要时刷新
for (int i = 0; i < 1000; ++i) {
    LOG_MAIN_INFO_AT("Message {}", i);
}
LogManager::getInstance().FlushAll();  // 循环结束后刷新一次
```

---

### 2. **线程安全**

`FlushAll()` 是线程安全的，可以在任何线程中调用：

```cpp
std::thread t1([&]() {
    LOG_MAIN_INFO_AT("Message from thread 1");
});

std::thread t2([&]() {
    LOG_MAIN_INFO_AT("Message from thread 2");
});

t1.join();
t2.join();

// 主线程刷新
LogManager::getInstance().FlushAll();
```

---

### 3. **与 Shutdown 的区别**

| 方法 | 功能 | 使用后能否继续记录日志 |
|------|------|---------------------|
| `FlushAll()` | 刷新缓冲区 | ✅ 可以 |
| `Shutdown()` | 刷新并关闭 | ❌ 不可以 |

```cpp
// FlushAll - 刷新后可以继续使用
LogManager::getInstance().FlushAll();
LOG_MAIN_INFO_AT("This will work");  // ✅

// Shutdown - 关闭后不能再用
LogManager::getInstance().Shutdown();
LOG_MAIN_INFO_AT("This will NOT work");  // ❌
```

---

## 📝 完整示例

```cpp
#include "log/logmanager.h"
#include <iostream>

int main() {
    try {
        // 1. 初始化 LogManager
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init("./logs", 1);
        
        LOG_MAIN_INFO_AT("=== Application Starting ===");
        
        // 2. 加载配置
        ConfigManager& config_mgr = ConfigManager::getInstance();
        if (!config_mgr.load("config.yaml")) {
            LOG_MAIN_WARN_AT("Config not found, using defaults");
        }
        
        // 3. 重新配置日志
        const auto& config = config_mgr.getConfig();
        if (config.logs.count("mainlog") > 0) {
            auto logger_cfg = log_utils::convertToLoggerConfig(
                config.logs.at("mainlog"));
            log_mgr.ReloadFromConfig(logger_cfg);
            LOG_MAIN_INFO_AT("Logger reconfigured");
        }
        
        // 4. 确保配置加载的日志已输出
        LogManager::getInstance().FlushAll();
        
        // 5. 运行应用
        auto& app = Application::getInstance();
        int result = app.run();
        
        // 6. 退出前刷新所有日志
        LOG_MAIN_INFO_AT("=== Application Exiting (code: {}) ===", result);
        LogManager::getInstance().FlushAll();
        
        return result;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Fatal error: {}", e.what());
        
        // 确保错误日志被写入
        LogManager::getInstance().FlushAll();
        
        return 1;
    }
}
```

---

## 🎯 最佳实践

### 1. **在关键节点刷新**

```cpp
// ✅ 推荐：在重要操作后刷新
void saveData() {
    database.save();
    LOG_MAIN_INFO_AT("Data saved successfully");
    LogManager::getInstance().FlushAll();  // 确保日志写入
}
```

---

### 2. **异常处理中刷新**

```cpp
// ✅ 推荐：捕获异常后立即刷新
try {
    riskyOperation();
} catch (...) {
    LOG_MAIN_ERROR_AT("Operation failed");
    LogManager::getInstance().FlushAll();  // 确保错误日志不丢失
    throw;
}
```

---

### 3. **程序退出前刷新**

```cpp
// ✅ 推荐：main 函数返回前刷新
int main() {
    // ... 应用程序逻辑
    
    LOG_MAIN_INFO_AT("Shutting down...");
    LogManager::getInstance().FlushAll();  // 最后刷新一次
    
    return 0;
}
```

---

### 4. **不要过度使用**

```cpp
// ❌ 避免：每条日志都刷新
LOG_MAIN_INFO_AT("Step 1");
LogManager::getInstance().FlushAll();  // 不必要的开销

LOG_MAIN_INFO_AT("Step 2");
LogManager::getInstance().FlushAll();  // 不必要的开销

// ✅ 推荐：批量操作后刷新一次
LOG_MAIN_INFO_AT("Step 1");
LOG_MAIN_INFO_AT("Step 2");
LOG_MAIN_INFO_AT("Step 3");
LogManager::getInstance().FlushAll();  // 只刷新一次
```

---

## 🔗 相关文档

- [CONFIG_LOG_UNIFICATION_TWOSTAGE.md](../config/CONFIG_LOG_UNIFICATION_TWOSTAGE.md) - Config 模块日志统一方案
- [DYNAMIC_CONFIG_FEATURES.md](../config/DYNAMIC_CONFIG_FEATURES.md) - 动态配置功能

---

## 📊 总结

### 优势

✅ **防止日志丢失** - 确保所有日志都被写入  
✅ **调试友好** - 立即看到日志输出  
✅ **线程安全** - 可在任何线程调用  
✅ **简单易用** - 一行代码即可  

---

### 适用场景

- ✅ 程序退出前
- ✅ 异常处理后
- ✅ 关键操作完成后
- ✅ 测试用例中
- ✅ 调试时

---

### 不适用场景

- ❌ 高频循环中（性能差）
- ❌ 实时性要求极高的场景
- ❌ 每条日志后都调用（没必要）
