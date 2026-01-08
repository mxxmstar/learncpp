#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <stdint.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class TCPSession : public std::enable_shared_from_this<TCPSession>
{
public:
    TCPSession(net::io_context& ioc);
    TCPSession(tcp::socket socket);
    virtual ~TCPSession();

    void Start();
    void Stop();
    void SetBufferSize(std::size_t size);
    std::string GetSessionID() const;
    std::string GetRemoteAddress() const;
    int16_t GetRemotePort() const;
protected:
    void async_read();
    void async_write(const uint8_t* data, std::size_t size);
    
    // 子类只关心“收到字节”
    virtual void on_bytes(const uint8_t* data, std::size_t size) = 0;
    virtual void on_error(boost::system::error_code ec) = 0;

    tcp::socket socket_;
    std::vector<uint8_t> read_buffer_;
    std::string session_id_;

};

