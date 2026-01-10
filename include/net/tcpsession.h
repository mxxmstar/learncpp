#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <stdint.h>
#include <queue>

#include "session.h"
#include "databuffer.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

class AsioTCPSession : protected Session, public std::enable_shared_from_this<AsioTCPSession>
{
public:
    using tcp = boost::asio::ip::tcp;
    using executor_type = boost::asio::any_io_executor;
    using stranc_type = boost::asio::strand<executor_type>;

    explicit AsioTCPSession(net::io_context& ioc);
    explicit AsioTCPSession(tcp::socket socket);
    ~AsioTCPSession();

    void Start() override;
    void Stop() override;
    void Send(std::shared_ptr<std::vector<uint8_t>> buf);
    void Send(const uint8_t* data, size_t size) override;


    bool IsRunning() const ;
    std::string GetSessionID() const;
    std::string GetRemoteAddress() const override;
    int16_t GetRemotePort() const override;
protected:
    void AsyncRead();
    void AsyncWrite();
    
    // 子类只关心“收到字节”
    void OnBytes(const uint8_t* data, std::size_t size) override;
    void OnClose() override;
    void OnError(boost::system::error_code ec);
        
    tcp::socket socket_;
    stranc_type strand_;

    std::vector<uint8_t> read_buffer_;
    std::string session_id_;
    std::atomic<bool> closed_{ false };
private:
    // 发送队列和相关变量
    std::queue<SendBuffer> send_queue_;
};

