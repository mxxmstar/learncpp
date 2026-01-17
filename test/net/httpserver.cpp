#include "net/tcpserver.h"
#include "net/tcpsession.h"
#include "net/asio_io_context_pool.h"
#include "log/logmanager.h"
#include "net/httpserver.h"
#include "net/httpsession.h"
#include <iostream>
#include <memory>

using namespace boost::asio::ip;


int main()
{
    try {
        LogManager& log_manager = LogManager::getInstance();
        log_manager.Init();
        std::cout << "LogManager initialized" << std::endl;
        
        // 创建主io_context
        boost::asio::io_context main_io_context;
        
        // 创建工作池
        auto& worker_pool = AsioIOContextPool::GetInstance(AsioIOContextPool::ServiceType::HTTP);
        
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