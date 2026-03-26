#pragma once

#include <memory>
#include <functional>
#include <atomic>
#include <boost/asio.hpp>
#include "net/websocket_session.h"

namespace Net {

class AsioWebSocketServer {
public:
    using ConnectHandler = std::function<void(std::shared_ptr<AsioWebSocketSession>)>;

    AsioWebSocketServer(boost::asio::io_context& io_context, uint16_t port);
    ~AsioWebSocketServer() = default;

    void Start();
    void Stop();

    void SetConnectHandler(ConnectHandler handler);

private:
    void DoAccept();

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> running_{false};

    ConnectHandler connect_handler_;
};

}
