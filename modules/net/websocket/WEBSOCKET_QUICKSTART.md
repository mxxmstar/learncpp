# WebSocket 模块 - 快速开始

## 5 分钟上手指南

### 1️⃣ 启动 WebSocket 服务器

```cpp
#include "net/websocket_server.h"
#include "net/websocket_router.h"
#include <boost/asio.hpp>

int main() {
    boost::asio::io_context io_context;
    
    // 创建服务器（端口 9090）
    Net::AsioWebSocketServer server(io_context, 9090);
    
    // 配置连接处理
    server.SetConnectHandler([](std::shared_ptr<Net::AsioWebSocketSession> session) {
        auto& router = Net::WebSocketRouter::GetInstance();
        
        // 绑定会话到路由器
        router.BindSession(session->GetSessionId(), session);
        
        // 设置消息处理
        session->SetMessageHandler([&router](const std::string& sid, 
                                              const std::string& msg) {
            std::cout << "Received: " << msg << std::endl;
            
            // 通过路由器分发消息
            router.DispatchMessage(sid, msg);
            
            // 回复客户端
            router.SendTo(sid, "Echo: " + msg);
        });
        
        // 设置关闭处理
        session->SetCloseHandler([&router](const std::string& sid) {
            router.UnbindSession(sid);
        });
        
        // 启动会话
        session->Start();
    });
    
    // 注册消息处理器
    auto& router = Net::WebSocketRouter::GetInstance();
    
    router.RegisterMessageHandler("hello", 
        [](const std::string& session_id, const std::string& message) {
            std::cout << "Hello from " << session_id << std::endl;
            router.SendTo(session_id, R"({"type":"welcome"})");
        });
    
    // 启动服务器
    server.Start();
    std::cout << "Server started on port 9090" << std::endl;
    
    // 运行
    io_context.run();
}
```

---

### 2️⃣ 浏览器客户端测试

打开浏览器控制台（F12），输入：

```javascript
// 连接服务器
const ws = new WebSocket('ws://127.0.0.1:9090');

// 连接成功
ws.onopen = () => {
    console.log('Connected!');
    
    // 发送 hello 消息
    ws.send(JSON.stringify({
        type: 'hello',
        name: 'Browser'
    }));
};

// 接收消息
ws.onmessage = (event) => {
    console.log('Received:', event.data);
};

// 发送普通消息
ws.send('Hello Server!');
```

---

### 3️⃣ C++ 客户端测试

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
    net::io_context io_context;
    
    // 解析地址
    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve("127.0.0.1", "9090");
    
    // 创建 WebSocket
    websocket::stream<tcp::socket> ws(io_context);
    
    // 连接
    net::connect(ws.next_layer(), endpoints.begin(), endpoints.end());
    ws.handshake("127.0.0.1", "/");
    
    std::cout << "Connected!" << std::endl;
    
    // 发送消息
    std::string msg = R"({"type":"hello","name":"C++ Client"})";
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
    
    return 0;
}
```

---

### 4️⃣ 运行项目测试

```bash
# 编译测试程序
cmake --build . --target test_net_websocket

# 运行测试（选择模式 1 - 服务器）
./bin/test_net_websocket.exe

# 在另一个终端运行测试（选择模式 2 - 客户端）
./bin/test_net_websocket.exe
```

---

## 📌 核心概念

### 三大组件

| 组件 | 作用 | 类比 |
|------|------|------|
| **AsioWebSocketServer** | 监听端口，接受连接 | 酒店前台 |
| **AsioWebSocketSession** | 管理单个客户端连接 | 房间服务员 |
| **WebSocketRouter** | 消息路由和会话管理 | 总机接线员 |

### 消息流转

```
客户端发送消息
    ↓
AsioWebSocketSession 接收
    ↓
调用 message_handler_
    ↓
WebSocketRouter::DispatchMessage 解析 type
    ↓
根据 type 找到对应的 handler
    ↓
执行自定义逻辑
    ↓
可选：回复客户端
```

---

## 💡 常用操作

### 发送文本消息

```cpp
session->Send("Hello!");
session->Send(R"({"type":"chat","content":"Hi"})");
```

### 发送二进制消息

```cpp
std::vector<uint8_t> data = {0x01, 0x02, 0x03};
session->SendBinary(data);
```

### 向指定客户端发送

```cpp
auto& router = Net::WebSocketRouter::GetInstance();
router.SendTo(session_id, "Private message");
```

### 广播消息（需要自己实现）

```cpp
void Broadcast(const std::string& message) {
    // 遍历所有会话并发送
    // 参考 WEBSOCKET_USAGE.md 中的示例
}
```

---

## ⚠️ 注意事项

1. **线程安全**
   - ✅ `Send()` 是线程安全的
   - ❌ 不要在回调中长时间阻塞
   - ❌ 避免同步 IO 操作

2. **消息格式**
   - 推荐使用 JSON
   - 必须包含 `type` 字段用于路由
   - 示例：`{"type":"chat","content":"Hello"}`

3. **生命周期**
   - Session 使用 `shared_ptr` 管理
   - Router 使用 `weak_ptr` 避免内存泄漏
   - 断开连接时自动清理

4. **错误处理**
   - 网络错误会自动记录日志
   - 建议在应用层实现重连
   - 检查返回的错误码

---

## 🔍 调试技巧

### 查看日志

```cpp
// 日志文件位置：./logs/main_YYYY-MM-DD.log
// 搜索关键字：WebSocket
```

### 启用详细日志

```cpp
LOG_INIT_MAIN("./logs", "test", spdlog::level::debug);
```

### 监控连接数

```cpp
// 在 Router 中添加
size_t GetActiveSessionCount() {
    std::lock_guard lock(mutex_);
    size_t count = 0;
    for (auto& pair : sessions_) {
        if (!pair.second.expired()) {
            ++count;
        }
    }
    return count;
}
```

---

## 📚 更多资源

- [完整使用文档](./WEBSOCKET_USAGE.md)
- [修复总结](./WEBSOCKET_FIX_SUMMARY.md)
- [测试代码](../../test/net/websocket.cpp)

---

**有问题？** 查看详细文档或运行测试程序学习更多用法！
