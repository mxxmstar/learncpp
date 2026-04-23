#include "net/websocket/websocket_session.h"
#include "log/logmanager.h"
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/stream.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

namespace Net {

AsioWebSocketSession::AsioWebSocketSession(tcp::socket&& socket)
    : session_id_(boost::uuids::to_string(boost::uuids::random_generator()()))
    , ws_(std::move(socket))
    , read_buffer_()
    , write_queue_()
    , is_writing_(false) {
}

void AsioWebSocketSession::Start() {
    auto self = shared_from_this();
    
    ws_.async_accept([this, self](boost::system::error_code ec) {
        if (!ec) {
            LOG_MAIN_INFO_AT("WebSocket session {} connected", session_id_);
            AsyncRead();
        } else {
            LOG_MAIN_ERROR_AT("WebSocket handshake failed: {}", ec.message());
        }
    });
}

void AsioWebSocketSession::Close() {
    if (!close_sent_) {
        close_sent_ = true;
        boost::system::error_code ec;
        ws_.close({}, ec);
    }
}

void AsioWebSocketSession::Send(const std::string& message) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    
    // 将消息复制到队列
    std::vector<uint8_t> data(message.begin(), message.end());
    write_queue_.push(std::move(data));
    
    // 如果当前没有正在进行的写操作，则开始写
    if (!is_writing_) {
        is_writing_ = true;
        DoWrite();
    }
}

void AsioWebSocketSession::SendBinary(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    
    // 将数据复制到队列
    write_queue_.push(data);
    
    // 如果当前没有正在进行的写操作，则开始写
    if (!is_writing_) {
        is_writing_ = true;
        DoWrite();
    }
}

void AsioWebSocketSession::SetMessageHandler(MessageHandler handler) {
    message_handler_ = std::move(handler);
}

void AsioWebSocketSession::SetCloseHandler(CloseHandler handler) {
    close_handler_ = std::move(handler);
}

void AsioWebSocketSession::AsyncRead() {
    auto self = shared_from_this();
    ws_.async_read(read_buffer_, [this, self](boost::system::error_code ec, std::size_t bytes) {
        OnRead(ec, bytes);
    });
}

void AsioWebSocketSession::OnRead(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec == websocket::error::closed) {
            LOG_MAIN_INFO_AT("WebSocket session {} closed by client", session_id_);
        } else {
            LOG_MAIN_ERROR_AT("WebSocket read error: {}", ec.message());
        }
        OnClose(ec);
        return;
    }

    if (message_handler_) {
        // 使用 buffer 的实际大小，而不是 bytes_transferred
        auto buffers = read_buffer_.data();
        std::string msg(static_cast<const char*>(buffers.data()), buffers.size());
        message_handler_(session_id_, msg);
    }

    read_buffer_.consume(read_buffer_.size());
    AsyncRead();
}

void AsioWebSocketSession::DoWrite() {
    if (write_queue_.empty()) {
        is_writing_ = false;
        return;
    }
    
    auto self = shared_from_this();
    auto& data = write_queue_.front();
    
    ws_.async_write(boost::asio::buffer(data), [this, self](boost::system::error_code ec, std::size_t) {
        std::lock_guard<std::mutex> lock(write_mutex_);
        
        if (ec) {
            LOG_MAIN_ERROR_AT("WebSocket write error: {}", ec.message());
            write_queue_.pop();
            is_writing_ = false;
            return;
        }
        
        // 移除已发送的数据
        write_queue_.pop();
        
        // 继续发送下一条消息
        DoWrite();
    });
}

void AsioWebSocketSession::OnWrite(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        LOG_MAIN_ERROR_AT("WebSocket write error: {}", ec.message());
    }
}

void AsioWebSocketSession::OnClose(boost::system::error_code ec) {
    if (close_handler_) {
        close_handler_(session_id_);
    }
}

}
