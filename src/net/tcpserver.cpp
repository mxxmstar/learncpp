#include "net/tcpserver.h"
#include "log/logmanager.h"
#include "net/asio_io_context_pool.h"

AsioTCPServer::AsioTCPServer(boost::asio::io_context& io_context, AsioIOContextPool& worker_pool, uint16_t port)
    : accept_ioc_(io_context), worker_pool_(worker_pool),
      acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) 
{
    LOG_MAIN_INFO("AsioTCPServer::AsioTCPServer()");   
}

void AsioTCPServer::Start() {
    LOG_MAIN_INFO_AT("AsioTCPServer::Start()");
    running_ = true;
    DoAccept();
}

void AsioTCPServer::Stop() {
    LOG_MAIN_INFO_AT("AsioTCPServer::Stop()");
    running_ = false;
    boost::system::error_code ec;
    acceptor_.close(ec);
}

void AsioTCPServer::SetAcceptHandler(AcceptHandler handler) {
    accept_handler_ = std::move(handler);
}

void AsioTCPServer::DoAccept() { 
    if (!running_) {
        return;
    }

    auto& ioc = worker_pool_.GetIOContext();
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(ioc);
    acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec) {
        if (!ec) {
            try {
                LOG_MAIN_INFO_AT("AsioTCPServer::DoAccept()");
                if (accept_handler_) {
                    accept_handler_(std::move(*socket));
                }
            } catch (std::exception& e) {
                LOG_MAIN_ERROR_AT("AsioTCPServer::DoAccept()", "Exception: %s", e.what());
            }
        }
        DoAccept();
    });
}
