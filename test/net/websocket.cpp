#include "net/websocket/websocket_server.h"
#include "net/websocket/websocket_router.h"
#include "log/logmanager.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <limits>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// 测试服务端
void TestWebSocketServer() {
    LOG_MAIN_INFO_AT("========== WebSocket Server Test ==========");
    
    net::io_context io_context;
    Net::AsioWebSocketServer server(io_context, 9090);
    
    // 设置连接处理
    server.SetConnectHandler([](std::shared_ptr<Net::AsioWebSocketSession> session) {
        LOG_MAIN_INFO_AT("[Server] New client connected: {}", session->GetSessionId());
        
        // 绑定到路由器
        Net::WebSocketRouter::GetInstance().BindSession(session->GetSessionId(), session);
        
        // 设置消息处理
        session->SetMessageHandler([](const std::string& session_id, const std::string& message) {
            LOG_MAIN_INFO_AT("[Server] Received from {}: {}", session_id, message);
            
            // 尝试通过路由器分发消息（需要 JSON 格式）
            Net::WebSocketRouter::GetInstance().DispatchMessage(session_id, message);
            
            // 回复消息（简单的 echo）
            std::string reply = "Echo: " + message;
            Net::WebSocketRouter::GetInstance().SendTo(session_id, reply);
        });
        
        // 设置关闭处理
        session->SetCloseHandler([](const std::string& session_id) {
            LOG_MAIN_INFO_AT("[Server] Client disconnected: {}", session_id);
            Net::WebSocketRouter::GetInstance().UnbindSession(session_id);
        });
    });
    
    // 注册消息处理器
    Net::WebSocketRouter::GetInstance().RegisterMessageHandler("chat", 
        [](const std::string& session_id, const std::string& message) {
            LOG_MAIN_INFO_AT("[Router] Chat message from {}: {}", session_id, message);
        });
    
    Net::WebSocketRouter::GetInstance().RegisterMessageHandler("ping",
        [](const std::string& session_id, const std::string& message) {
            LOG_MAIN_INFO_AT("[Router] Ping from {}", session_id);
            Net::WebSocketRouter::GetInstance().SendTo(session_id, R"({"type":"pong"})");
        });
    
    // 设置连接/断开回调
    Net::WebSocketRouter::GetInstance().SetConnectHandler(
        [](const std::string& session_id) {
            LOG_MAIN_INFO_AT("[Router] Client registered: {}", session_id);
        });
    
    Net::WebSocketRouter::GetInstance().SetDisconnectHandler(
        [](const std::string& session_id) {
            LOG_MAIN_INFO_AT("[Router] Client unregistered: {}", session_id);
        });
    
    server.Start();
    
    LOG_MAIN_INFO_AT("[Server] WebSocket server started on port 9090");
    LOG_MAIN_INFO_AT("[Server] Press Enter to stop...");
    LOG_MAIN_INFO_AT("[Server] Tip: Send JSON messages like {{\"type\":\"ping\"}} for routing");
    
    // 在后台线程运行 io_context
    std::thread io_thread([&io_context]() {
        io_context.run();
    });
    
    // 等待用户输入
    std::cin.get();
    
    // 停止服务器和 io_context
    server.Stop();
    io_context.stop();
    
    // 等待 IO 线程结束
    if (io_thread.joinable()) {
        io_thread.join();
    }
}

// 测试客户端
void TestWebSocketClient() {
    LOG_MAIN_INFO_AT("========== WebSocket Client Test ==========");
    
    try {
        net::io_context io_context;
        tcp::resolver resolver(io_context);
        websocket::stream<tcp::socket> ws(io_context);
        
        // 连接到服务器
        auto endpoints = resolver.resolve("127.0.0.1", "9090");
        net::connect(ws.next_layer(), endpoints.begin(), endpoints.end());
        
        // WebSocket 握手
        ws.handshake("127.0.0.1", "/");
        
        LOG_MAIN_INFO_AT("[Client] Connected to server");
        
        // 发送文本消息
        std::string msg1 = R"({"type":"chat","content":"Hello WebSocket!"})";
        ws.write(net::buffer(msg1));
        LOG_MAIN_INFO_AT("[Client] Sent: {}", msg1);
        
        // 接收响应
        beast::flat_buffer buffer;
        ws.read(buffer);
        std::string response1(static_cast<const char*>(buffer.data().data()), buffer.size());
        LOG_MAIN_INFO_AT("[Client] Received: {}", response1);
        buffer.consume(buffer.size());
        
        // 发送 ping
        std::string msg2 = R"({"type":"ping"})";
        ws.write(net::buffer(msg2));
        LOG_MAIN_INFO_AT("[Client] Sent: {}", msg2);
        
        // 接收 pong
        ws.read(buffer);
        std::string response2(static_cast<const char*>(buffer.data().data()), buffer.size());
        LOG_MAIN_INFO_AT("[Client] Received: {}", response2);
        buffer.consume(buffer.size());
        
        // 发送二进制数据
        std::vector<uint8_t> binary_data = {0x01, 0x02, 0x03, 0x04, 0x05};
        ws.write(net::buffer(binary_data));
        LOG_MAIN_INFO_AT("[Client] Sent binary data (5 bytes)");
        
        // 关闭连接
        ws.close(websocket::close_code::normal);
        LOG_MAIN_INFO_AT("[Client] Connection closed");
        
    } catch (std::exception& e) {
        LOG_MAIN_ERROR_AT("[Client] Error: {}", e.what());
    }
}

// 并发写入测试
void TestConcurrentWrite() {
    LOG_MAIN_INFO_AT("========== Concurrent Write Test ==========");
    
    try {
        net::io_context io_context;
        tcp::resolver resolver(io_context);
        websocket::stream<tcp::socket> ws(io_context);
        
        // 连接
        auto endpoints = resolver.resolve("127.0.0.1", "9090");
        net::connect(ws.next_layer(), endpoints.begin(), endpoints.end());
        ws.handshake("127.0.0.1", "/");
        
        LOG_MAIN_INFO_AT("[Client] Connected for concurrent test");
        
        // 启动多个线程并发发送
        const int thread_count = 5;
        const int messages_per_thread = 10;
        std::vector<std::thread> threads;
        
        for (int i = 0; i < thread_count; ++i) {
            threads.emplace_back([&ws, i, messages_per_thread]() {
                for (int j = 0; j < messages_per_thread; ++j) {
                    std::string msg = "Thread-" + std::to_string(i) + "-Msg-" + std::to_string(j);
                    try {
                        ws.write(net::buffer(msg));
                        LOG_MAIN_INFO_AT("[Thread {}] Sent: {}", i, msg);
                        
                        // 接收响应
                        beast::flat_buffer buffer;
                        ws.read(buffer);
                        std::string response(static_cast<const char*>(buffer.data().data()), buffer.size());
                        LOG_MAIN_INFO_AT("[Thread {}] Received: {}", i, response);
                        buffer.consume(buffer.size());
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    } catch (std::exception& e) {
                        LOG_MAIN_ERROR_AT("[Thread {}] Error: {}", i, e.what());
                    }
                }
            });
        }
        
        // 等待所有线程完成
        for (auto& t : threads) {
            t.join();
        }
        
        // 关闭
        ws.close(websocket::close_code::normal);
        LOG_MAIN_INFO_AT("[Client] Concurrent test completed");
        
    } catch (std::exception& e) {
        LOG_MAIN_ERROR_AT("[Client] Error: {}", e.what());
    }
}

int main() {
    // 初始化日志
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();
    
    LOG_MAIN_INFO_AT("Choose test mode:");
    LOG_MAIN_INFO_AT("1. Server only (manual client testing)");
    LOG_MAIN_INFO_AT("2. Client only (requires running server)");
    LOG_MAIN_INFO_AT("3. Concurrent write test (requires running server)");
    LOG_MAIN_INFO_AT("Enter choice (1/2/3): ");
    
    int choice;
    std::cin >> choice;
    
    // 清除输入缓冲区中的残留字符（包括换行符）
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    
    switch (choice) {
        case 1:
            TestWebSocketServer();
            break;
        case 2:
            TestWebSocketClient();
            break;
        case 3:
            TestConcurrentWrite();
            break;
        default:
            LOG_MAIN_ERROR_AT("Invalid choice!");
            return 1;
    }
    
    return 0;
}
