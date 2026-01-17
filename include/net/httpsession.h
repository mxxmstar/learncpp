#pragma once
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;
namespace net = boost::asio;
/// @brief HTTP短连接会话类，每个连接对应一个会话，包含一个超时定时器，60秒未收到请求则关闭连接
class AsioHttpSession : public std::enable_shared_from_this<AsioHttpSession>
{    
public:
    explicit AsioHttpSession(tcp::socket&& socket);
    ~AsioHttpSession();
    void Start();
private:    
    void AsyncRead();
    void AsyncWrite();
    void AsyncCheckDeadline();

    void HandleRequest();

    

    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> rsp_;
    /// @brief 超时定时器，60秒未收到请求则关闭连接
    net::steady_timer deadline_timer_ { socket_.get_executor(), std::chrono::seconds(60) };
    std::string url_;
};

