#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <functional>

namespace Net {
class AsioIOContextPool;

class AsioTCPServer {
public:
    using AcceptHandler = std::function<void(boost::asio::ip::tcp::socket)>;
    AsioTCPServer(boost::asio::io_context& io_context, AsioIOContextPool& worker_pool, uint16_t port);
    virtual ~AsioTCPServer() = default;

    void Start();
    void Stop();

    void SetAcceptHandler(AcceptHandler handler);

    std::string GetServerIp() const;
    uint16_t GetServerPort() const;

    boost::asio::ip::tcp::endpoint GetServerEndpoint() const;
private:    
    void DoAccept();

    boost::asio::io_context& accept_ioc_;
    AsioIOContextPool& worker_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> running_{false};
    AcceptHandler accept_handler_;
};

}