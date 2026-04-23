#include "net/tcp_server/tcpserver.h"
#include "net/tcp_server/tcpsession.h"
#include "net/io_context_pool/asio_io_context_pool.h"
#include "log/logmanager.h"
#include "net/http_server/http_server.h"
#include "net/http_server/http_session.h"
#include <iostream>
#include <memory>

using namespace boost::asio::ip;
using namespace Net;

int main()
{
    try {
        LogManager& log_manager = LogManager::getInstance();
        log_manager.Init();
        std::cout << "LogManager initialized" << std::endl;
        
        // 创建主io_context
        boost::asio::io_context main_io_context;
        
        // 创建工作池
        auto& worker_pool = AsioIOContextPool::GetInstance();
        
        // 创建HTTP服务器，监听端口8080
        AsioHttpServer server(main_io_context, worker_pool, 8080);
        
        // 启动服务器
        server.Start();
        
        std::cout << "HTTP server started on port 8080" << std::endl;
        
        // 运行主io_context
        main_io_context.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0; 
}