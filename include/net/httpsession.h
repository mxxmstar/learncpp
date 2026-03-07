#pragma once
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;
namespace net = boost::asio;

class IAsioSession : public std::enable_shared_from_this<IAsioSession>
{
public:
    explicit IAsioSession(tcp::socket&& socket) : socket_(std::move(socket)) {};
    virtual ~IAsioSession() = default;
    virtual void Start() = 0;
	virtual std::string GetSessionId() const { 
        return boost::uuids::to_string(boost::uuids::random_generator()()); 
    }
    virtual void Close() {
        boost::system::error_code ec;      
        socket_.close(ec);
	}
protected:    
    tcp::socket socket_;
};

/// @brief HTTP短连接会话类，每个连接对应一个会话，包含一个超时定时器，60秒未收到请求则关闭连接
class AsioHttpSession : public IAsioSession
{    
public:
    explicit AsioHttpSession(tcp::socket&& socket);
    ~AsioHttpSession();
    void Start() override;
private:    
    void AsyncRead();
    void AsyncWrite();
    void AsyncCheckDeadline();

    void HandleRequest();
        
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> rsp_;
    /// @brief 超时定时器，60秒未收到请求则关闭连接
    net::steady_timer deadline_timer_ { socket_.get_executor(), std::chrono::seconds(60) };    
};

