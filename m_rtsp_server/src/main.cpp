#include <iostream>
#include <rtsp_session.h>

int main() {
    //try {
    //    LogManager& log_manager = LogManager::getInstance();
    //    log_manager.Init();
    //    std::cout << "LogManager initialized" << std::endl;
    //    // 创建主io_context
    //    boost::asio::io_context main_io_context;

    //    // 创建工作池
    //    auto& worker_pool = AsioIOContextPool::GetInstance(AsioIOContextPool::ServiceType::TCP);

    //    // 创建Echo服务器，监听端口8888
    //    EchoTCPServer server(main_io_context, worker_pool, 8888);

    //    // 启动服务器
    //    server.Start();

    //    std::cout << "Echo server started on port 8888" << std::endl;

    //    // 运行主io_context
    //    main_io_context.run();

    //}
    //catch (const std::exception& e) {
    //    std::cerr << "Exception: " << e.what() << std::endl;
    //}
    M_RTSPSession s;
    std::cout << "111" << std::endl;
    return 0;
}