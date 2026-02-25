#include "net/httpserver.h"
#include "net/httpsession.h"
#include "log/logmanager.h"
AsioHttpServer::AsioHttpServer(boost::asio::io_context& io_context, AsioIOContextPool& worker_pool, uint16_t port)
    : accept_ioc_(io_context), worker_pool_(worker_pool),
      acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))     
{
}


void AsioHttpServer::Start() {
    LOG_MAIN_INFO_AT("AsioHttpServer::Start()");
    running_ = true;
    DoAccept();
}

void AsioHttpServer::Stop() {
    running_ = false;
    boost::system::error_code ec;
    acceptor_.close(ec);
}


void AsioHttpServer::DoAccept() {
    if (!running_) {
        return;
    }

    auto& ioc = worker_pool_.GetIOContext();
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(ioc);
    acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec) {
        if (!ec) {
            try {                
                auto session = std::make_shared<AsioHttpSession>(std::move(*socket));
                LOG_MAIN_INFO_AT("AsioHTTPServer::DoAccept(), session {:p} started", fmt::ptr(session.get()));
                session->Start();
            } catch (std::exception& e) {
                LOG_MAIN_ERROR_AT("AsioHTTPServer::DoAccept()", "Exception: %s", e.what());
            }
        }
        DoAccept();
    });
}