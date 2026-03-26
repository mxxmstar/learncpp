#include "net/websocket_session.h"
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
    , write_buffer_() {
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
    auto self = shared_from_this();
    ws_.async_write(boost::asio::buffer(message), [this, self](boost::system::error_code ec, std::size_t) {
        if (ec) {
            LOG_MAIN_ERROR_AT("WebSocket write error: {}", ec.message());
        }
    });
}

void AsioWebSocketSession::SendBinary(const std::vector<uint8_t>& data) {
    auto self = shared_from_this();
    ws_.async_write(boost::asio::buffer(data), [this, self](boost::system::error_code ec, std::size_t) {
        if (ec) {
            LOG_MAIN_ERROR_AT("WebSocket write binary error: {}", ec.message());
        }
    });
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
        std::string msg(static_cast<const char*>(read_buffer_.data().data()), bytes_transferred);
        message_handler_(session_id_, msg);
    }

    read_buffer_.consume(bytes_transferred);
    AsyncRead();
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
