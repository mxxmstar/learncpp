#include "hello_grpc_service.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <functional>

using namespace grpc_module;

// 全局标志用于控制测试
static bool g_running = true;

void SignalHandler(int signal) {
    std::cout << "\n[SignalHandler] Received signal: " << signal << std::endl;
    g_running = false;
}

/// @brief 测试 0：和 python GRPC 客户端的通信测试
void TestServerWithPython() {
    std::cout << "\n========== Test 0: Server Start ==========" << std::endl;

    HelloGrpcServer server("0.0.0.0:50051");

    std::cout << "Starting server..." << std::endl;
    bool success = server.Start();
    if (!success) {
        std::cerr << "Failed to start server" << std::endl;
        return;
    }

    std::cout << "Server is running: " << (server.IsRunning() ? "Yes" : "No") << std::endl;

    //// 等待一段时间
    //std::cout << "Server will run for 3 seconds..." << std::endl;
    //std::this_thread::sleep_for(std::chrono::seconds(3));

    //std::cout << "Stopping server..." << std::endl;
    //server.Stop();
    server.Wait();

    //std::cout << "Server stopped. Test passed!" << std::endl;
}

/// @brief 测试 1：启动和停止服务器
void TestServerLifecycle() {
    std::cout << "\n========== Test 1: Server Lifecycle ==========" << std::endl;
    
    HelloGrpcServer server("0.0.0.0:50051");
    
    std::cout << "Starting server..." << std::endl;
    bool success = server.Start();
    if (!success) {
        std::cerr << "Failed to start server" << std::endl;
        return;
    }
    
    std::cout << "Server is running: " << (server.IsRunning() ? "Yes" : "No") << std::endl;
    
    // 等待一段时间
    std::cout << "Server will run for 3 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    std::cout << "Stopping server..." << std::endl;
    server.Stop();
    server.Wait();
    
    std::cout << "Server stopped. Test passed!" << std::endl;
}

/// @brief 测试 2：客户端连接测试
void TestClientConnection() {
    std::cout << "\n========== Test 2: Client Connection ==========" << std::endl;
    
    HelloGrpcServer server("0.0.0.0:50052");
    server.Start();
    
    // 给服务器时间启动
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    HelloGrpcClient client("localhost:50052");
    
    std::cout << "Waiting for connection..." << std::endl;
    bool connected = client.WaitForConnected(5);
    
    if (connected) {
        std::cout << "Connection successful!" << std::endl;
        
        // 检查连接状态
        auto state = client.GetState(false);
        std::cout << "Channel state: " << state << std::endl;
    } else {
        std::cerr << "Connection failed!" << std::endl;
    }
    
    server.Stop();
    server.Wait();
    
    std::cout << "Test completed!" << std::endl;
}

/// @brief 测试 3：Unary RPC - SayHello
void TestUnaryRPC() {
    std::cout << "\n========== Test 3: Unary RPC (SayHello) ==========" << std::endl;
    
    HelloGrpcServer server("0.0.0.0:50053");
    server.Start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    HelloGrpcClient client("localhost:50053");
    
    if (!client.WaitForConnected(5)) {
        std::cerr << "Failed to connect to server" << std::endl;
        server.Stop();
        server.Wait();
        return;
    }
    
    simple_grpc::HelloResponse response;
    bool success = client.SayHello("World", &response);
    
    if (success) {
        std::cout << "Response message: " << response.message() << std::endl;
        std::cout << "Response timestamp: " << response.timestamp() << std::endl;
    } else {
        std::cerr << "RPC call failed" << std::endl;
    }
    
    // 测试另一个请求
    std::cout << "\nTesting another request..." << std::endl;
    simple_grpc::HelloResponse response2;
    success = client.SayHello("gRPC", &response2);
    
    if (success) {
        std::cout << "Response message: " << response2.message() << std::endl;
    }
    
    server.Stop();
    server.Wait();
    
    std::cout << "Test completed!" << std::endl;
}

/// @brief 测试 4：Server Streaming RPC - SayHelloStream
void TestServerStreaming() {
    std::cout << "\n========== Test 4: Server Streaming (SayHelloStream) ==========" << std::endl;
    
    HelloGrpcServer server("0.0.0.0:50054");
    server.Start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    HelloGrpcClient client("localhost:50054");
    
    if (!client.WaitForConnected(5)) {
        std::cerr << "Failed to connect to server" << std::endl;
        server.Stop();
        server.Wait();
        return;
    }
    
    // 收集所有响应
    std::vector<std::string> messages;
    
    // 调用流式 RPC
    bool success = client.SayHelloStream(
        "StreamTest",
        5,  // 请求 5 条消息
        [&](const simple_grpc::HelloResponse& response) {
            messages.push_back(response.message());
            std::cout << "Callback received: " << response.message() << std::endl;
        },
        10000  // 10 秒超时
    );
    
    if (success) {
        std::cout << "\nAll received messages:" << std::endl;
        for (size_t i = 0; i < messages.size(); ++i) {
            std::cout << "  " << (i + 1) << ". " << messages[i] << std::endl;
        }
    } else {
        std::cerr << "Streaming RPC failed" << std::endl;
    }
    
    server.Stop();
    server.Wait();
    
    std::cout << "Test completed!" << std::endl;
}

/// @brief 测试 5：多个客户端并发请求
void TestConcurrentClients() {
    std::cout << "\n========== Test 5: Concurrent Clients ==========" << std::endl;
    
    HelloGrpcServer server("0.0.0.0:50055");
    server.Start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    const int num_clients = 5;
    std::vector<std::thread> threads;
    std::vector<std::string> results(num_clients);
    
    std::cout << "Creating " << num_clients << " concurrent clients..." << std::endl;
    
    for (int i = 0; i < num_clients; ++i) {
        threads.emplace_back([&results, i]() {
            HelloGrpcClient client("localhost:50055");
            
            if (!client.WaitForConnected(5)) {
                results[i] = "Connection failed";
                return;
            }
            
            simple_grpc::HelloResponse response;
            std::string client_name = "Client_" + std::to_string(i + 1);
            
            if (client.SayHello(client_name, &response)) {
                results[i] = response.message();
            } else {
                results[i] = "RPC failed";
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "\nResults from all clients:" << std::endl;
    for (int i = 0; i < num_clients; ++i) {
        std::cout << "  Client " << (i + 1) << ": " << results[i] << std::endl;
    }
    
    server.Stop();
    server.Wait();
    
    std::cout << "Test completed!" << std::endl;
}

/// @brief 综合测试：启动服务器，然后运行所有测试
void RunAllTests() {
    std::cout << "========================================" << std::endl;
    std::cout << "gRPC Hello Service Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        TestServerLifecycle();
        
        TestClientConnection();
        
        TestUnaryRPC();
        
        TestServerStreaming();
        
        TestConcurrentClients();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "All tests completed successfully!" << std::endl;
        std::cout << "========================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\nError during tests: " << e.what() << std::endl;
    }
}

int main() {
#ifdef _WIN32
    // Windows 信号处理
    // 在 Windows 上，信号处理有限制，这里简单处理
#endif
    
    RunAllTests();
    // TestServerWithPython();
    
    return 0;
}
