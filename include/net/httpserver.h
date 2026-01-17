#pragma once
#include <boost/asio.hpp>
#include "net/asio_io_context_pool.h"
namespace net = boost::asio;

class AsioHttpServer {
public:
    AsioHttpServer(boost::asio::io_context& io_context, AsioIOContextPool& worker_pool, uint16_t port);
    void Start();
    void Stop();
private:
    void DoAccept();

    boost::asio::io_context& accept_ioc_;
    AsioIOContextPool& worker_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> running_{false};
    
};