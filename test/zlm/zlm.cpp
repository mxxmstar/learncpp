#include "zlmediakit/zlm_hookserver.h"
#include <iostream>
#include "log/logmanager.h"
#include "net/httpserver.h"
#include "net/httprouter.h"

int main() {
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();

    std::cout << "Initializing ZLMediaKit Hook Server Test..." << std::endl;
    
    try {

        auto r = HttpRouter::GetInstance();
        r.RegisterRoute("/hook/server_started", [](const boost::json::object& req_obj, boost::json::object& rsp_obj){
            rsp_obj["code"] = 200;
            rsp_obj["msg"] = "server_started";
            std::cout << "server_started" << std::endl;
        });

        // 创建主io_context
        boost::asio::io_context main_io_context;
        
        // 创建工作池
        auto& worker_pool = AsioIOContextPool::GetInstance(AsioIOContextPool::ServiceType::HTTP);
        
        // 创建HTTP服务器，监听端口8080
        AsioHttpServer server(main_io_context, worker_pool, 8080);
        
        // 启动服务器
        server.Start();

        // 运行主io_context
        main_io_context.run();
        
        // 创建 Hook 处理器，使用默认密钥
        ZLMHookHandler hook_handler("test_secret", nullptr);
        
        
    
        
        std::cout << "ZLMediaKit Hook Server is running!" << std::endl;

        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        LOG_MAIN_ERROR_AT("ZLM Hook Server test failed: {}", e.what());
        return 1;
    }
    
    std::cout << "ZLMediaKit Hook Server test completed." << std::endl;
    return 0;
}
