#include "rtsp/rtspserver.h"
#include "rtsp/rtspmacro.h"
#include "rtp/mediasession.h"
namespace rtsp {

RtspServer::RtspServer(boost::asio::io_context& io_context, 
                       AsioIOContextPool& work_pool, 
                       uint16_t port)
    : server_(io_context, work_pool, port) {
    LOG_RTSP_INFO_AT("RtspServer created on port {}", port);
}

void RtspServer::Start() {
    LOG_RTSP_INFO_AT("RtspServer starting...");
    server_.SetAcceptHandler([this](boost::asio::ip::tcp::socket socket) {
        OnCreateSession(std::move(socket));
    });
    server_.Start();
}

void RtspServer::OnCreateSession(boost::asio::ip::tcp::socket socket) {
    try {
        LOG_RTSP_INFO_AT("New RTSP connection from: {}", 
                        socket.remote_endpoint().address().to_string());

        auto session = std::make_shared<RtspSession>(std::move(socket));
        std::vector<std::shared_ptr<rtp::MediaSource>> sources;
        sources.push_back(std::make_shared<rtp::H264Source>(25));
        session->SetMediaSources(sources);
        session->SetCloseHandler([this, session]() {
            LOG_RTSP_INFO_AT("RTSP session closed: {}", session->GetSessionID());
        });

        session->Start();
    } catch (std::exception& e) {
        LOG_RTSP_ERROR_AT("Failed to create RTSP session: {}", e.what());
    }
}

}
