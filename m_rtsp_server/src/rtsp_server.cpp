#include "net/tcpserver.h"
#include "rtsp_server.h"
#include "rtsp_session.h"
#include "rtsp_log.h"

namespace mx {
RtspServer::RtspServer(boost::asio::io_context& io_context, AsioIOContextPool& pool, uint16_t port) : server_(io_context, pool, port) {
    LOG_RTSP_INFO_AT("RtspServer::RtspServer");
}

void RtspServer::Start() {
    LOG_RTSP_INFO_AT("RtspServer::Start");
    server_.SetAcceptHandler([this](boost::asio::ip::tcp::socket socket) {
        OnCreateSession(std::move(socket));
    });
    server_.Start();
}

void RtspServer::OnCreateSession(boost::asio::ip::tcp::socket socket) {
    try {
        LOG_RTSP_INFO_AT("RtspServer::OnCreateSession, remote_ip: {}", socket.remote_endpoint().address().to_string());

        // 创建 RTSP 会话
        auto session = std::make_shared<RtspSession>(std::move(socket));

        session->SetDataHandler([this, session](const uint8_t* data, size_t size) {
			// LOG_RTSP_INFO_AT("RtspServer::OnCreateSession, received data: {} bytes", size);
			// LOG_RTSP_INFO_AT("RtspServer::OnCreateSession, data: {}", std::string(reinterpret_cast<const char*>(data), size));
        });
        session->SetCloseHandler([this, session]() { 
            LOG_RTSP_INFO_AT("RtspServer::OnCreateSession, session closed.");
        });

        session->Start();
    }
    catch (std::exception& e) {
        LOG_RTSP_ERROR_AT("RtspServer::OnCreateSession, remote_ip: {}, error: {}", socket.remote_endpoint().address().to_string(), e.what());
    }    
}
}