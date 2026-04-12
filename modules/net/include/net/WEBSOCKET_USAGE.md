# WebSocket 模块使用文档

## 概述

本项目的 WebSocket 模块基于 Boost.Beast 实现，提供了完整的 WebSocket 服务器、会话管理和消息路由功能。支持文本和二进制消息的收发，具备并发安全的写入队列机制。

## 架构设计

```
┌─────────────────────────────────────────────┐
│          AsioWebSocketServer                 │
│  - 监听端口                                  │
│  - 接受连接                                  │
│  - 创建 Session                              │
└──────────────┬──────────────────────────────┘
               │
               │ 新连接
               ▼
┌─────────────────────────────────────────────┐
│        AsioWebSocketSession                  │
│  - WebSocket 握手                            │
│  - 异步读写                                  │
│  - 并发写入队列                              │
│  - 消息/关闭回调                             │
└──────────────┬──────────────────────────────┘
               │
               │ 消息分发
               ▼
┌─────────────────────────────────────────────┐
│         WebSocketRouter                      │
│  - 单例模式                                  │
│  - 消息类型路由                              │
│  - 会话管理 (weak_ptr)                       │
│  - 连接/断开回调                             │
└─────────────────────────────────────────────┘
```

## 核心组件

### 1. AsioWebSocketServer

WebSocket 服务器类，负责监听端口和接受客户端连接。

**主要接口：**

```cpp
// 构造函数
AsioWebSocketServer(boost::asio::io_context& io_context, uint16_t port);

// 启动服务器
void Start();

// 停止服务器
void Stop();

// 设置连接处理器
void SetConnectHandler(ConnectHandler handler);
```

**使用示例：**

```cpp
#include "net/websocket_server.h"

boost::asio::io_context io_context;
Net::AsioWebSocketServer server(io_context, 9090);

// 设置连接处理
server.SetConnectHandler([](std::shared_ptr<Net::AsioWebSocketSession> session) {
    std::cout << "New client: " << session->GetSessionId() << std::endl;
    
    // 配置 session...
    session->Start();
});

server.Start();
io_context.run();
```

### 2. AsioWebSocketSession

WebSocket 会话类，管理单个客户端连接的生命周期。

**主要特性：**

- ✅ 自动生成唯一的 Session ID（UUID）
- ✅ 并发安全的写入队列（避免数据竞争）
- ✅ 异步读写操作
- ✅ 支持文本和二进制消息
- ✅ 优雅关闭机制

**主要接口：**

```cpp
// 获取会话 ID
std::string GetSessionId() const;

// 发送文本消息
void Send(const std::string& message);

// 发送二进制消息
void SendBinary(const std::vector<uint8_t>& data);

// 设置消息处理器
void SetMessageHandler(MessageHandler handler);

// 设置关闭处理器
void SetCloseHandler(CloseHandler handler);

// 关闭连接
void Close();
```

**使用示例：**

```cpp
// 在连接处理器中配置 session
server.SetConnectHandler([](std::shared_ptr<Net::AsioWebSocketSession> session) {
    // 设置消息处理
    session->SetMessageHandler([](const std::string& session_id, 
                                   const std::string& message) {
        std::cout << "Received from " << session_id << ": " << message << std::endl;
        
        // 回复消息
        session->Send("Echo: " + message);
    });
    
    // 设置关闭处理
    session->SetCloseHandler([](const std::string& session_id) {
        std::cout << "Client disconnected: " << session_id << std::endl;
    });
    
    // 启动会话
    session->Start();
});
```

### 3. WebSocketRouter

消息路由器（单例），提供基于消息类型的分发和会话管理。

**主要功能：**

- 📋 注册不同类型的消息处理器
- 🔗 管理所有活跃的会话（使用 weak_ptr 避免内存泄漏）
- 📨 向指定会话发送消息
- 🔔 连接/断开事件通知

**主要接口：**

```cpp
// 获取单例实例
static WebSocketRouter& GetInstance();

// 注册消息处理器（按消息类型）
void RegisterMessageHandler(const std::string& msg_type, MessageHandler handler);

// 设置连接/断开处理器
void SetConnectHandler(ConnectHandler handler);
void SetDisconnectHandler(DisconnectHandler handler);

// 分发消息（解析 JSON 中的 type 字段）
void DispatchMessage(const std::string& session_id, const std::string& message);

// 向指定会话发送消息
void SendTo(const std::string& session_id, const std::string& message);

// 绑定/解绑会话
void BindSession(const std::string& session_id, 
                 std::shared_ptr<AsioWebSocketSession> session);
void UnbindSession(const std::string& session_id);
```

**使用示例：**

```cpp
#include "net/websocket_router.h"

auto& router = Net::WebSocketRouter::GetInstance();

// 注册聊天消息处理器
router.RegisterMessageHandler("chat", 
    [](const std::string& session_id, const std::string& message) {
        std::cout << "Chat from " << session_id << ": " << message << std::endl;
        
        // 广播给所有客户端（需要额外实现）
        // ...
    });

// 注册 ping 消息处理器
router.RegisterMessageHandler("ping",
    [](const std::string& session_id, const std::string& message) {
        router.SendTo(session_id, R"({"type":"pong"})");
    });

// 设置连接回调
router.SetConnectHandler([](const std::string& session_id) {
    std::cout << "Client connected: " << session_id << std::endl;
});

// 设置断开回调
router.SetDisconnectHandler([](const std::string& session_id) {
    std::cout << "Client disconnected: " << session_id << std::endl;
});
```

## 完整示例

### 服务端

```cpp
#include "net/websocket_server.h"
#include "net/websocket_router.h"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    boost::asio::io_context io_context;
    
    // 创建服务器
    Net::AsioWebSocketServer server(io_context, 9090);
    
    // 配置连接处理
    server.SetConnectHandler([](std::shared_ptr<Net::AsioWebSocketSession> session) {
        auto session_id = session->GetSessionId();
        std::cout << "New connection: " << session_id << std::endl;
        
        // 绑定到路由器
        Net::WebSocketRouter::GetInstance().BindSession(session_id, session);
        
        // 设置消息处理
        session->SetMessageHandler([](const std::string& sid, 
                                       const std::string& msg) {
            // 通过路由器分发消息
            Net::WebSocketRouter::GetInstance().DispatchMessage(sid, msg);
        });
        
        // 设置关闭处理
        session->SetCloseHandler([](const std::string& sid) {
            Net::WebSocketRouter::GetInstance().UnbindSession(sid);
        });
        
        // 启动会话
        session->Start();
    });
    
    // 注册消息处理器
    auto& router = Net::WebSocketRouter::GetInstance();
    
    router.RegisterMessageHandler("hello", 
        [](const std::string& session_id, const std::string& message) {
            std::cout << "Hello message: " << message << std::endl;
            router.SendTo(session_id, R"({"type":"welcome","msg":"Hello!"})");
        });
    
    router.RegisterMessageHandler("chat",
        [](const std::string& session_id, const std::string& message) {
            std::cout << "Chat: " << message << std::endl;
            router.SendTo(session_id, R"({"type":"echo","msg":"Received"})");
        });
    
    // 启动服务器
    server.Start();
    std::cout << "WebSocket server started on port 9090" << std::endl;
    
    // 运行 IO 上下文
    io_context.run();
    
    return 0;
}
```

### 客户端（Boost.Beast）

```cpp
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

int main() {
    try {
        net::io_context io_context;
        
        // 解析地址
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "9090");
        
        // 创建 WebSocket 流
        websocket::stream<tcp::socket> ws(io_context);
        
        // 连接
        net::connect(ws.next_layer(), endpoints.begin(), endpoints.end());
        
        // WebSocket 握手
        ws.handshake("127.0.0.1", "/");
        
        std::cout << "Connected to server" << std::endl;
        
        // 发送消息
        std::string msg = R"({"type":"hello","name":"Client"})";
        ws.write(net::buffer(msg));
        std::cout << "Sent: " << msg << std::endl;
        
        // 接收响应
        beast::flat_buffer buffer;
        ws.read(buffer);
        std::string response(static_cast<const char*>(buffer.data().data()), 
                            buffer.size());
        std::cout << "Received: " << response << std::endl;
        
        // 关闭
        ws.close(websocket::close_code::normal);
        
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### 客户端（JavaScript - 浏览器）

```javascript
// 创建 WebSocket 连接
const ws = new WebSocket('ws://127.0.0.1:9090');

// 连接打开
ws.onopen = function() {
    console.log('Connected to server');
    
    // 发送 hello 消息
    ws.send(JSON.stringify({
        type: 'hello',
        name: 'Browser Client'
    }));
};

// 接收消息
ws.onmessage = function(event) {
    console.log('Received:', event.data);
    
    const data = JSON.parse(event.data);
    
    if (data.type === 'welcome') {
        console.log('Welcome message:', data.msg);
        
        // 发送聊天消息
        ws.send(JSON.stringify({
            type: 'chat',
            content: 'Hello from browser!'
        }));
    }
};

// 连接关闭
ws.onclose = function() {
    console.log('Connection closed');
};

// 错误处理
ws.onerror = function(error) {
    console.error('WebSocket error:', error);
};
```

## 消息格式约定

推荐使用 JSON 格式的消息，包含 `type` 字段用于路由：

```json
{
    "type": "message_type",
    "data": {
        // 具体数据
    }
}
```

**示例：**

```json
// 聊天消息
{"type": "chat", "content": "Hello!", "timestamp": 1234567890}

// Ping/Pong
{"type": "ping"}
{"type": "pong"}

// 系统通知
{"type": "notification", "level": "info", "message": "System ready"}
```

## 并发安全

### 写入队列机制

`AsioWebSocketSession` 内部实现了线程安全的写入队列：

```
Thread 1: Send("msg1") ──┐
                          ├──► Write Queue ──► async_write (序列化)
Thread 2: Send("msg2") ──┘
```

**特点：**

- ✅ 多线程可以安全地调用 `Send()` / `SendBinary()`
- ✅ 消息按顺序发送，不会交错
- ✅ 使用互斥锁保护队列操作
- ✅ 自动管理写状态，避免并发写入

**注意事项：**

- ⚠️ 不要在消息处理器中长时间阻塞（会影响其他消息的处理）
- ⚠️ 如果需要大量发送消息，考虑批量处理或使用背压机制

## 会话管理

### Weak Pointer 策略

`WebSocketRouter` 使用 `std::weak_ptr` 管理会话，避免循环引用导致的内存泄漏：

```cpp
std::unordered_map<std::string, std::weak_ptr<AsioWebSocketSession>> sessions_;
```

**优势：**

- ✅ Session 销毁时自动从 map 中清理
- ✅ 发送前检查指针有效性
- ✅ 避免内存泄漏

**清理机制：**

```cpp
void SendTo(const std::string& session_id, const std::string& message) {
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        auto session = it->second.lock();  // 尝试提升为 shared_ptr
        if (session) {
            session->Send(message);
        } else {
            // 指针已过期，清理
            sessions_.erase(it);
        }
    }
}
```

## 测试

项目提供了完整的测试程序：`test/net/websocket.cpp`

**编译和运行：**

```bash
# 编译（需要在 CMakeLists.txt 中添加测试）
cmake --build . --target test_websocket

# 运行测试
./bin/test_websocket
```

**测试模式：**

1. **Server only** - 仅启动服务器，可手动用其他客户端测试
2. **Client only** - 作为客户端连接到运行中的服务器
3. **Concurrent write test** - 并发写入测试（5个线程，每个发送10条消息）

**测试内容：**

- ✅ 基本连接和断开
- ✅ 文本消息收发
- ✅ 二进制消息收发
- ✅ 消息路由（基于 type 字段）
- ✅ 并发写入安全性
- ✅ 会话管理（绑定/解绑）
- ✅ 弱指针清理

## 常见问题

### Q1: 为什么发送消息没有立即发出？

**A:** 消息会被放入写入队列，按顺序异步发送。这是为了保证并发安全和消息顺序。

### Q2: 如何处理大量并发连接？

**A:** 
- 使用 `boost::asio::io_context` 线程池
- 限制每个 session 的消息队列长度
- 实现背压机制（当队列过长时拒绝新消息）

### Q3: 如何广播消息给所有客户端？

**A:** 遍历 `sessions_` map：

```cpp
void Broadcast(const std::string& message) {
    std::lock_guard lock(mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        auto session = it->second.lock();
        if (session) {
            session->Send(message);
            ++it;
        } else {
            it = sessions_.erase(it);  // 清理过期会话
        }
    }
}
```

### Q4: 如何实现心跳检测？

**A:** 定期发送 ping 消息并等待 pong：

```cpp
// 在服务端
void StartHeartbeat(std::shared_ptr<AsioWebSocketSession> session) {
    auto timer = std::make_shared<boost::asio::steady_timer>(io_context);
    
    auto check = [session, timer]() {
        // 发送 ping
        session->Send(R"({"type":"ping"})");
        
        // 设置超时
        timer->expires_after(std::chrono::seconds(30));
        timer->async_wait([session](const boost::system::error_code& ec) {
            if (!ec) {
                // 超时，关闭连接
                session->Close();
            }
        });
    };
    
    // 每 30 秒检查一次
    timer->expires_after(std::chrono::seconds(30));
    timer->async_wait([check](const boost::system::error_code& ec) {
        if (!ec) {
            check();
        }
    });
}
```

### Q5: WebSocket 和 HTTP 可以共用端口吗？

**A:** 可以，但需要额外的协议升级逻辑。当前实现是独立的 WebSocket 服务器。如需混合服务，可以考虑：

- 使用 Boost.Beast 的 HTTP + WebSocket 组合
- 在前端使用 Nginx 反向代理
- 在不同的端口上分别运行

## 性能优化建议

1. **使用 IO 上下文线程池**
   ```cpp
   boost::asio::thread_pool pool(4);  // 4 个线程
   boost::asio::io_context& io_context = pool;
   ```

2. **限制消息大小**
   ```cpp
   ws_.set_option(websocket::stream_base::decorator(
       [](websocket::response_type& res) {
           res.set(http::field::server, "MyWebSocket/1.0");
       }));
   ```

3. **批量发送小消息**
   - 将多个小消息合并为一个大的二进制帧
   - 减少系统调用次数

4. **监控队列长度**
   - 如果写入队列持续增长，说明发送速度跟不上
   - 实现背压或丢弃策略

## 更新日志

### 修复的问题

1. ✅ **读取缓冲区大小错误** - 修正了 `OnRead` 中使用 `bytes_transferred` 而非 `buffer.size()` 的问题
2. ✅ **并发写入数据竞争** - 添加了写入队列和互斥锁，确保线程安全
3. ✅ **弱指针未清理** - 在 `SendTo` 中添加了过期指针的清理逻辑
4. ✅ **缺少 DoWrite 方法** - 实现了序列化的异步写入流程

## 参考资料

- [Boost.Beast WebSocket 文档](https://www.boost.org/doc/libs/release/libs/beast/doc/html/beast/ref/boost__beast__websocket.html)
- [Boost.Asio 异步编程](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)
- [WebSocket RFC 6455](https://datatracker.ietf.org/doc/html/rfc6455)
