#include "net/websocket/websocket_server.h"
#include "log/logmanager.h"

namespace Net {

AsioWebSocketServer::AsioWebSocketServer(boost::asio::io_context& io_context, uint16_t port)
    : io_context_(io_context)
    , acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
}

void AsioWebSocketServer::Start() {
    LOG_MAIN_INFO_AT("AsioWebSocketServer starting on port {}", acceptor_.local_endpoint().port());
    running_ = true;
    DoAccept();
}

void AsioWebSocketServer::Stop() {
    running_ = false;
    boost::system::error_code ec;
    acceptor_.close(ec);
    LOG_MAIN_INFO_AT("AsioWebSocketServer stopped");
}

void AsioWebSocketServer::SetConnectHandler(ConnectHandler handler) {
    connect_handler_ = std::move(handler);
}

void AsioWebSocketServer::DoAccept() {
    if (!running_) {
        return;
    }

    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
    acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec) {
        if (!ec) {
            try {
                auto session = std::make_shared<AsioWebSocketSession>(std::move(*socket));
                LOG_MAIN_INFO_AT("WebSocket session {} accepted", session->GetSessionId());
                
                if (connect_handler_) {
                    connect_handler_(session);
                }
                
                session->Start();
            } catch (std::exception& e) {
                LOG_MAIN_ERROR_AT("WebSocket accept exception: {}", e.what());
            }
        }
        DoAccept();
    });
}

}
