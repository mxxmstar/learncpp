#include "net/tcp_server/tcpsession.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "log/logmanager.h"
namespace Net {
static constexpr std::size_t kReadBufferSize = 16 * 1024;
using tcp = boost::asio::ip::tcp;

AsioTCPSession::AsioTCPSession(boost::asio::io_context& ioc) : socket_(ioc), read_buffer_(kReadBufferSize)
{
    boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();
    session_id_ = boost::uuids::to_string(a_uuid);
}

AsioTCPSession::AsioTCPSession(tcp::socket socket) : socket_(std::move(socket)), read_buffer_(kReadBufferSize)
{
    boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();
    session_id_ = boost::uuids::to_string(a_uuid);
}

AsioTCPSession::~AsioTCPSession() {
    Stop();
}

void AsioTCPSession::Start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        LOG_MAIN_DEBUG_AT("session is running");
        return;
    }
    AsyncRead();
}

void AsioTCPSession::Stop() {
    bool expected = false;
    if (!closed_.compare_exchange_strong(expected, true)) {
        LOG_MAIN_DEBUG_AT("session is closed");
        return;
    }

    boost::system::error_code ec;
    socket_.cancel(ec);
    socket_.shutdown(tcp::socket::shutdown_type::shutdown_both, ec);
    socket_.close(ec);
    running_ = false;
    OnClose();
}

void AsioTCPSession::Send(const std::string& data) {
    Send(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

void AsioTCPSession::Send(const uint8_t* data, size_t size) {
    if (!data || size == 0 || !running_) {
        return;
    }

    auto buf = std::make_shared<std::vector<uint8_t>>(data, data + size);
    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(), [this, self, buf, size]() {
        if (closed_) {
            return;
        }
        LOG_MAIN_DEBUG_AT("session {} send {} bytes, data: {}", session_id_, size, std::string(reinterpret_cast<const char*>(buf->data()), size));
        send_queue_.emplace(buf);
        if (!writing_) {
            writing_ = true;
            AsyncWrite();
        }
        });
}

void AsioTCPSession::AsyncRead() {
    if (!running_) {
        return;
    }

    auto self = shared_from_this();
    socket_.async_read_some(
        boost::asio::buffer(read_buffer_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (ec || bytes_transferred == 0) {
                LOG_MAIN_CRITICAL_AT("session {} async_read_some error: {}", session_id_, ec.message());
                Stop();
                return;
            }

            OnBytes(read_buffer_.data(), bytes_transferred);
            // 继续读取
            AsyncRead();
        }
    );
}

void AsioTCPSession::AsyncWrite() {
    if (!running_ || send_queue_.empty()) {
        return;
    }

    auto self = shared_from_this();
    auto& buf = send_queue_.front();
    boost::asio::async_write(socket_, boost::asio::buffer(*buf),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                LOG_MAIN_CRITICAL_AT("session {} async_write error: {}", session_id_, ec.message());
                Stop();
                return;
            }

            send_queue_.pop();
            if (!send_queue_.empty()) {
                AsyncWrite();
            }
            else {
                writing_ = false;   // 队列非空，重置发送标志
            }
        }
    );
}

int AsioTCPSession::NativeFd() {
    return socket_.native_handle();
}

tcp::socket AsioTCPSession::DetachSocket() {
    StopRead();
    running_ = false;
    return std::move(socket_);
}

void AsioTCPSession::StopRead() {
    boost::system::error_code ec;
    socket_.cancel(ec);
}

boost::asio::io_context& AsioTCPSession::GetIOContext() {
    return static_cast<boost::asio::io_context&>(socket_.get_executor().context());
}

void AsioTCPSession::OnBytes(const uint8_t* data, size_t size) {
    if (data_handler_) {
        data_handler_(data, size);
    }
}

void AsioTCPSession::OnClose() {
    if (close_handler_) {
        close_handler_();
    }
}

void AsioTCPSession::OnError(boost::system::error_code ec) {
    LOG_MAIN_ERROR_AT("session {} error: {}", session_id_, ec.message());
    Stop();
}

bool AsioTCPSession::IsRunning() const {
    return running_.load();
}

std::string AsioTCPSession::GetSessionID() const {
    return session_id_;
}

std::string AsioTCPSession::GetRemoteAddress() const {
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return "";
    }
    return socket_.remote_endpoint().address().to_string();
}

int16_t AsioTCPSession::GetRemotePort() const {
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return -1;
    }
    return socket_.remote_endpoint().port();
}

std::string AsioTCPSession::GetLocalAddress() const {
    boost::system::error_code ec;
    auto endpoint = socket_.local_endpoint(ec);
    if (ec) {
        return "";
    }
    return endpoint.address().to_string();
}

int16_t AsioTCPSession::GetLocalPort() const {
    boost::system::error_code ec;
    auto endpoint = socket_.local_endpoint(ec);
    if (ec) {
        return -1;
    }
    return endpoint.port();
}

void AsioTCPSession::SetDataHandler(DataHandler handler) {
    data_handler_ = std::move(handler);
}

void AsioTCPSession::SetCloseHandler(CloseHandler handler) {
    close_handler_ = std::move(handler);
}

}