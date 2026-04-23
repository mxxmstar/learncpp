#pragma once

#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <functional>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "net/websocket/websocket.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

namespace Net {

class AsioWebSocketSession : public std::enable_shared_from_this<AsioWebSocketSession> {
public:
    explicit AsioWebSocketSession(tcp::socket&& socket);
    ~AsioWebSocketSession() = default;

    void Start();
    void Close();

    std::string GetSessionId() const { return session_id_; }

    void Send(const std::string& message);
    void SendBinary(const std::vector<uint8_t>& data);

    using MessageHandler = std::function<void(const std::string& session_id, const std::string& message)>;
    void SetMessageHandler(MessageHandler handler);

    using CloseHandler = std::function<void(const std::string& session_id)>;
    void SetCloseHandler(CloseHandler handler);

private:
    void AsyncRead();
    void OnRead(boost::system::error_code ec, std::size_t bytes_transferred);
    void DoWrite();
    void OnWrite(boost::system::error_code ec, std::size_t bytes_transferred);
    void OnClose(boost::system::error_code ec);

    std::string session_id_;
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer read_buffer_;
    
    // 写入队列和锁，确保并发安全
    std::queue<std::vector<uint8_t>> write_queue_;
    bool is_writing_ = false;
    std::mutex write_mutex_;

    MessageHandler message_handler_;
    CloseHandler close_handler_;

    bool close_sent_ = false;
};

}
