# HttpClientPool 使用指南

## 概述

`HttpClientPool` 是一个 HTTP 客户端连接池，用于高效管理多个 HTTP 客户端连接，支持异步请求、资源自动管理和灵活的回调处理机制。

## 核心特性

- ✅ **连接池管理**：复用 TCP 连接，减少握手开销
- ✅ **RAII 资源管理**：自动获取和释放客户端，防止资源泄漏
- ✅ **异步非阻塞**：基于 Boost.Asio 的异步 I/O
- ✅ **灵活的回调机制**：支持自定义 Handler 和装饰器模式
- ✅ **线程安全**：支持多线程并发访问
- ✅ **统计监控**：提供连接池状态统计信息

## 快速开始

### 1. 基本使用示例

```cpp
#include "net/httpclientpool.h"
#include <boost/asio.hpp>
#include <boost/json.hpp>

int main() {
    // 创建 io_context
    boost::asio::io_context io_context;
    
    // 配置连接池
    HttpClientPool::Config config;
    config.host = "httpbin.org";
    config.port = 80;
    config.init_size = 5;      // 初始连接数
    config.max_size = 20;      // 最大连接数
    
    // 初始化连接池（单例）
    auto& pool = HttpClientPool::GetInstance();
    pool.Init(io_context, config);
    
    // 获取客户端（使用 RAII 守卫，自动释放）
    auto client_guard = pool.AcquireGuard();
    if (!client_guard) {
        std::cout << "Failed to acquire client" << std::endl;
        return 1;
    }
    
    // 发送POST请求
    boost::json::object req_obj;
    req_obj["message"] = "Hello World";
    
    client_guard->PostJsonWithHandler("/post", req_obj, 
        [](bool success, const boost::json::object& rsp) {
            if (success) {
                std::cout << "Request succeeded!" << std::endl;
                std::cout << "Response: " << rsp << std::endl;
            } else {
                std::cout << "Request failed!" << std::endl;
            }
        });
    
    // 运行 io_context 处理异步请求
    io_context.run();
    
    // 停止连接池
    pool.Stop();
    
    return 0;
}
```

### 2. 使用工厂函数创建 Handler

```cpp
// 设置全局的 Handler 工厂
pool.SetHandlerFactory([]() -> HttpClientPool::CompleteHandler {
    return [](bool success, boost::json::object& rsp) {
        if (success) {
            LOG_INFO("Request completed successfully");
        } else {
            LOG_ERROR("Request failed");
        }
    };
});

// 使用工厂方法发起请求（自动应用工厂创建的 handler）
client_guard->PostJson("/api/data", req_obj, pool.GetHandlerFactory());
```

### 3. 添加装饰器（横切关注点）

```cpp
// 添加日志装饰器（所有请求都会记录日志）
pool.AddHandlerDecorator([](HttpClientPool::CompleteHandler& handler) {
    auto start_time = std::chrono::steady_clock::now();
    
    auto original = std::move(handler);
    handler = [start_time, original = std::move(original)](
        bool success, boost::json::object& rsp) mutable {
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        
        LOG_INFO("Request took {}ms", duration);
        
        if (original) {
            original(success, rsp);
        }
    };
});

// 添加性能监控装饰器
pool.AddHandlerDecorator([](HttpClientPool::CompleteHandler& handler) {
    static std::atomic<int> request_count{0};
    
    auto original = std::move(handler);
    handler = [&request_count, original = std::move(original)](
        bool success, boost::json::object& rsp) mutable {
        
        int count = ++request_count;
        LOG_DEBUG("Total requests: {}", count);
        
        if (original) {
            original(success, rsp);
        }
    };
});
```

## API 参考

### HttpClientPool::Config

连接池配置参数：

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `host` | `std::string` | `"127.0.0.1"` | 目标服务器主机名 |
| `port` | `uint16_t` | `80` | 目标服务器端口 |
| `init_size` | `std::size_t` | `5` | 初始连接的客户端数量 |
| `max_size` | `std::size_t` | `20` | 连接池最大容量 |
| `connect_timeout_ms` | `int` | `30000` | 连接超时时间（毫秒） |
| `idle_timeout_sec` | `int` | `300` | 空闲连接超时时间（秒） |
| `max_requests_per_client` | `std::size_t` | `100` | 每个客户端最大请求次数 |

### HttpClientPool::PoolStats

连接池统计信息：

```cpp
struct PoolStats {
    std::size_t total;           // 当前池中客户端总数
    std::size_t available;       // 可用客户端数
    std::size_t active;          // 正在使用的客户端数
    std::size_t current_created; // 当前已创建数量
    std::size_t lifetime_created;// 历史累计创建数量
    std::size_t lifetime_destroyed; // 历史累计销毁数量
};
```

使用示例：

```cpp
auto stats = pool.GetStats();
std::cout << "Total: " << stats.total << std::endl;
std::cout << "Available: " << stats.available << std::endl;
std::cout << "Active: " << stats.active << std::endl;
```

### 核心方法

#### Init()

初始化连接池：

```cpp
void Init(boost::asio::io_context& io_context, const Config& config);
```

#### AcquireGuard()

获取客户端（推荐方式，RAII 自动管理）：

```cpp
PooledClientGuard AcquireGuard();
```

#### Acquire() / Release()

手动获取和释放客户端（不推荐，需要手动管理）：

```cpp
std::shared_ptr<PooledClient> Acquire();
void Release(std::shared_ptr<PooledClient> client);
```

#### SetHandlerFactory() / GetHandlerFactory()

设置和获取全局 Handler 工厂：

```cpp
void SetHandlerFactory(HandlerFactory factory);
HandlerFactory GetHandlerFactory() const;
```

#### AddHandlerDecorator()

添加全局 Handler 装饰器：

```cpp
void AddHandlerDecorator(HandlerDecorator decorator);
```

#### CreateDecoratedHandler()

创建装饰后的 Handler：

```cpp
CompleteHandler CreateDecoratedHandler(CompleteHandler base_handler);
```

#### Stop()

停止连接池，清理所有资源：

```cpp
void Stop();
```

### PooledClient 方法

#### PostJson() / GetJson()

发送 JSON 请求：

```cpp
// 方式 1：直接传递 handler
void PostJson(const std::string& url, const boost::json::object& req_obj,
              AsioAsyncHttpClient::CompleteHandler handler, int timeout_ms = 5000);

// 方式 2：使用工厂函数
void PostJson(const std::string& url, const boost::json::object& req_obj,
              HandlerFactory handler_factory, int timeout_ms = 5000);
```

#### PostJsonWithHandler() / GetJsonWithHandler()

直接指定 handler（不应用装饰器）：

```cpp
void PostJsonWithHandler(const std::string& url, const boost::json::object& req_obj,
                        CompleteHandler handler, int timeout_ms = 5000);

void GetJsonWithHandler(const std::string& url, CompleteHandler handler,
                       int timeout_ms = 5000);
```

## 使用场景

### 场景 1：ZLMediaKit API 客户端（无回调）

```cpp
class ZLMApiClient {
public:
    void GetApiList() {
        auto& pool = HttpClientPool::GetInstance();
        auto guard = pool.AcquireGuard();
        
        std::string url = "/index/api/getApiList?secret=" + secret_;
        
        // 使用空 handler，不需要回调处理
        guard->GetJsonWithHandler(url, AsioAsyncHttpClient::CompleteHandler());
    }
};
```

### 场景 2：业务请求（带错误处理）

```cpp
class BusinessService {
public:
    void SendData(const std::string& data, std::function<void(bool)> cb) {
        auto& pool = HttpClientPool::GetInstance();
        auto guard = pool.AcquireGuard();
        
        boost::json::object req;
        req["data"] = data;
        
        // 自定义错误处理逻辑
        guard->PostJsonWithHandler("/api/send", req, 
            [cb = std::move(cb)](bool success, const boost::json::object& rsp) {
                if (!success) {
                    LOG_ERROR("Send failed");
                    cb(false);
                    return;
                }
                
                // 检查业务错误码
                int code = rsp.at("code").as_int64();
                if (code != 0) {
                    LOG_ERROR("Business error: {}", code);
                    cb(false);
                    return;
                }
                
                LOG_INFO("Send succeeded");
                cb(true);
            });
    }
};
```

### 场景 3：混合使用（全局装饰器 + 自定义处理）

```cpp
// 程序启动时配置
void InitHttpClient() {
    auto& pool = HttpClientPool::GetInstance();
    
    // 配置连接池
    HttpClientPool::Config config;
    config.host = "api.example.com";
    config.port = 443;
    pool.Init(io_context, config);
    
    // 添加全局日志装饰器
    pool.AddHandlerDecorator([](auto& handler) {
        auto original = std::move(handler);
        handler = [original = std::move(original)](bool success, auto& rsp) mutable {
            LOG_INFO("[HTTP] Request: {}", success ? "OK" : "FAIL");
            if (original) original(success, rsp);
        };
    });
}

// 业务代码
void DoRequest() {
    auto& pool = HttpClientPool::GetInstance();
    auto guard = pool.AcquireGuard();
    
    // 自动应用全局日志装饰器
    guard->PostJson("/api/test", req_obj, pool.GetHandlerFactory());
}
```

## 最佳实践

### ✅ 推荐使用的方式

1. **使用 RAII 守卫管理资源**
```cpp
auto guard = pool.AcquireGuard();  // ✅ 自动释放
guard->PostJson(...);
// guard 离开作用域时自动归还到池中
```

2. **在程序启动时初始化一次**
```cpp
void ApplicationStart() {
    auto& pool = HttpClientPool::GetInstance();
    pool.Init(io_context, config);
    pool.SetHandlerFactory(CreateDefaultHandler());
    pool.AddHandlerDecorator(CreateLoggingDecorator());
}
```

3. **根据场景选择合适的接口**
```cpp
// 需要全局装饰器：使用 PostJson + 工厂
guard->PostJson(url, req, pool.GetHandlerFactory());

// 完全自定义：使用 PostJsonWithHandler
guard->PostJsonWithHandler(url, req, custom_handler);
```

### ❌ 避免的做法

1. **不要手动管理客户端释放**
```cpp
// ❌ 容易忘记释放
auto client = pool.Acquire();
client->PostJson(...);
// pool.Release(client); 忘记了！

// ✅ 使用 RAII
auto guard = pool.AcquireGuard();
guard->PostJson(...);
// 自动释放
```

2. **不要在每次请求时都设置工厂**
```cpp
// ❌ 效率低下
for (int i = 0; i < 100; ++i) {
    pool.SetHandlerFactory(my_factory);
    guard->PostJson(...);
}

// ✅ 提前设置一次
pool.SetHandlerFactory(my_factory);
for (int i = 0; i < 100; ++i) {
    guard->PostJson(..., pool.GetHandlerFactory());
}
```

3. **不要忘记运行 io_context**
```cpp
guard->PostJson(...);
// ❌ 忘记运行，异步请求不会执行
// io_context.run();

// ✅ 确保运行
io_context.run();
```

## 常见问题

### Q: 连接池是线程安全的吗？

A: 是的，`HttpClientPool` 内部使用互斥锁保护共享资源，支持多线程并发访问。

### Q: 如何调整连接池大小？

A: 通过配置文件中的 `init_size` 和 `max_size` 参数：

```cpp
config.init_size = 10;  // 启动时创建 10 个连接
config.max_size = 50;   // 最多允许 50 个连接
```

### Q: 如何处理请求超时？

A: 可以在请求时指定超时时间：

```cpp
guard->PostJson(url, req_obj, handler, 10000);  // 10 秒超时
```

### Q: 装饰器和工厂可以同时使用吗？

A: 可以。工厂负责创建 handler，装饰器负责包装 handler。调用顺序：

```
工厂创建 handler → 装饰器包装 → 实际执行
```

### Q: 如何在测试中使用？

A: 参考 `test/net/httpclientpool.cpp` 中的测试用例：

```cpp
TEST(HttpClientPool, BasicUsage) {
    boost::asio::io_context io_context;
    HttpClientPool::Config config;
    config.host = "httpbin.org";
    config.port = 80;
    
    auto& pool = HttpClientPool::GetInstance();
    pool.Init(io_context, config);
    
    auto guard = pool.AcquireGuard();
    ASSERT_TRUE(guard);
    ASSERT_TRUE(guard->IsValid());
    
    // 发送测试请求...
    
    pool.Stop();
}
```

## 相关文件

- **头文件**: `include/net/httpclientpool.h`
- **实现**: `src/net/httpclientpool.h`
- **底层客户端**: `include/net/httpclient.h`
- **测试**: `test/net/httpclientpool.cpp`
- **使用示例**: `src/zlmediakit/zlm_httpclient.cpp`

## 更新日志

### v1.0.0 (当前版本)

- ✅ 支持连接池管理
- ✅ 支持 RAII 资源管理
- ✅ 支持 Handler 工厂模式
- ✅ 支持装饰器模式
- ✅ 支持统计监控
- ✅ 线程安全

---

**最后更新**: 2026-03-28  
**维护者**: Development Team
