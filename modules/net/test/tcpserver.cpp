#include "net/tcp_server/tcpserver.h"
#include "net/tcp_server/tcpsession.h"
#include "net/io_context_pool/asio_io_context_pool.h"
#include "common/log/logmanager.h"
#include <iostream>
#include <memory>
#include <string>
using namespace boost::asio::ip;
using namespace Net;
class EchoSession : public AsioTCPSession {
public:
    explicit EchoSession(tcp::socket socket)
        : AsioTCPSession(std::move(socket)) {}

protected:
    void OnBytes(const uint8_t* data, size_t size) override {
        // 回显数据
        std::cout << "Received: " << std::string(reinterpret_cast<const char*>(data), size) << std::endl;
        Send(data, size);
    }

    void OnClose() override {
        LOG_MAIN_INFO_AT("EchoSession closed: {}", GetSessionID());
    }

};

class EchoTCPServer {    
public:
    EchoTCPServer(boost::asio::io_context& io_context, AsioIOContextPool& worker_pool, uint16_t port)
        : server_(io_context, worker_pool, port) {
           server_.SetAcceptHandler([this, &worker_pool](tcp::socket socket) {
               // 从工作池中获取一个io_context用于处理此连接
                auto& io_context = worker_pool.GetIOContext();
                
                // 创建EchoSession并启动
                auto session = std::make_shared<EchoSession>(std::move(socket));
                session->Start();
                
                LOG_MAIN_INFO_AT("New echo session created: {}", session->GetSessionID());
           });
    }
    void Start() {
        server_.Start();
    }
    
    void Stop() {
        server_.Stop();
    }        

private:
    AsioTCPServer server_;
};

int main() {
     try {
        LogManager& log_manager = LogManager::getInstance();
        log_manager.Init();
        std::cout << "LogManager initialized" << std::endl;
        // 创建主io_context
        boost::asio::io_context main_io_context;
        
        // 创建工作池
        auto& worker_pool = AsioIOContextPool::GetInstance();
        
        // 创建Echo服务器，监听端口8888
        EchoTCPServer server(main_io_context, worker_pool, 8888);
        
        // 启动服务器
        server.Start();
        
        std::cout << "Echo server started on port 8888" << std::endl;
        
        // 运行主io_context
        main_io_context.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}