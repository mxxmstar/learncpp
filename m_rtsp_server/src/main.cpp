#include "net/tcpserver.h"
#include "net/tcpsession.h"
#include "net/asio_io_context_pool.h"
#include "log/logmanager.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include "rtsp_log.h"
#include "rtsp_session.h"
#include "rtsp_server.h"
using namespace boost::asio::ip;

//class EchoSession : public AsioTCPSession {
//public:
//    explicit EchoSession(tcp::socket socket)
//        : AsioTCPSession(std::move(socket)) {}
//
//protected:
//    void OnBytes(const uint8_t* data, size_t size) override {
//        // 回显数据
//        std::cout << "Received: " << std::string(reinterpret_cast<const char*>(data), size) << std::endl;
//        Send(data, size);
//    }
//
//    void OnClose() override {
//        LOG_MAIN_INFO_AT("EchoSession closed: {}", GetSessionID());
//    }
//
//};
//
//class EchoTCPServer {
//public:
//    EchoTCPServer(boost::asio::io_context& io_context, AsioIOContextPool& worker_pool, uint16_t port)
//        : server_(io_context, worker_pool, port) {
//        server_.SetAcceptHandler([this, &worker_pool](tcp::socket socket) {
//            // 从工作池中获取一个io_context用于处理此连接
//            auto& io_context = worker_pool.GetIOContext();
//
//            // 创建EchoSession并启动
//            auto session = std::make_shared<EchoSession>(std::move(socket));
//            session->Start();
//
//            LOG_MAIN_INFO_AT("New echo session created: {}", session->GetSessionID());
//            });
//    }
//    void Start() {
//        server_.Start();
//    }
//
//    void Stop() {
//        server_.Stop();
//    }
//
//private:
//    AsioTCPServer server_;
//};

int main() {
    RtspLogger& logger = RtspLogger::GetInstance();
    logger.Init();
	LOG_RTSP_INFO_AT("RTSP server started");
    LOG_RTSP_INFO_AT("RTSP server started");


        // 创建主io_context
    boost::asio::io_context main_io_context;

    // 创建工作池
    auto& worker_pool = AsioIOContextPool::GetInstance(AsioIOContextPool::ServiceType::TCP);

	mx::RtspServer server(main_io_context, worker_pool, 8554);
    server.Start();

	main_io_context.run();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    spdlog::drop_all();
    return 0;
}
