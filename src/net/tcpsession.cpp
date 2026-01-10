#include "net/tcpsession.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "log/logmanager.h"

static constexpr std::size_t kReadBufferSize = 16 * 1024;

AsioTCPSession::AsioTCPSession(net::io_context& ioc) : socket_(ioc), strand_(socket_.get_executor()), read_buffer_(kReadBufferSize)
{
    boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();
	session_id_ = boost::uuids::to_string(a_uuid);
}

AsioTCPSession::AsioTCPSession(tcp::socket socket) : socket_(std::move(socket)), strand_(socket_.get_executor()), read_buffer_(kReadBufferSize)
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
    
    boost::asio::post(strand_, [self = shared_from_this()]() {
        self->AsyncRead();
    });
}

void AsioTCPSession::Stop() {
    bool expected = false;
    if (!closed_.compare_exchange_strong(expected, true)) {
        LOG_MAIN_DEBUG_AT("session is closed");
        return;
    }
    boost::asio::post(strand_, [self = shared_from_this()]() {
        boost::system::error_code ec;
        self->socket_.shutdown(tcp::socket::shutdown_type::shutdown_both, ec);
        self->socket_.close(ec);
        self->running_ = false;
        self->OnClose();
    });
}

void AsioTCPSession::AsyncRead() { 
    if (!running_) {
        return;
    }

    auto self = shared_from_this();
    socket_.async_read_some(
        boost::asio::buffer(read_buffer_), 
        boost::asio::bind_executor(strand_, [this, self](boost::system::error_code ec, std::size_t bytes_transferred) { 
            if (ec || bytes_transferred == 0) {
                LOG_MAIN_CRITICAL_AT("session {} async_read_some error: {}", session_id_, ec.message());
                Stop();
                return;
            }

            OnBytes(read_buffer_.data(), bytes_transferred);
            // 继续读取
            AsyncRead();
        })
    );
}

void AsioTCPSession::AsyncWrite() { 
    if (!running_ || send_queue_.empty()) {
        return;
    }

    auto self = shared_from_this();
    boost::asio::async_write(socket_, send_queue_.front().GetAsioConstBuffer(),
        boost::asio::bind_executor(strand_, [this, self](boost::system::error_code ec, std::size_t bytes_transferred) { 
            if (ec) {
                LOG_MAIN_CRITICAL_AT("session {} async_write error: {}", session_id_, ec.message());
                Stop();
                return;
            }

            send_queue_.pop();
            if (!send_queue_.empty()) {
                AsyncWrite();
            }
        })
    );
}

void AsioTCPSession::OnBytes(const uint8_t* data, size_t size) {
    
}

void AsioTCPSession::OnClose() {
    
}

void AsioTCPSession::OnError(boost::system::error_code ec) {
    LOG_MAIN_ERROR_AT("session {} error: {}", session_id_, ec.message());
    Stop();
}

void AsioTCPSession::Send(std::shared_ptr<std::vector<uint8_t>> buf) {
    Send(buf->data(), buf->size());   
}

void AsioTCPSession::Send(const uint8_t* data, size_t size) {
    if (!data || size == 0 || !running_) {
        return;
    }

    auto buf = std::make_shared<std::vector<uint8_t>>(data, data + size);
    auto self = shared_from_this();
    boost::asio::post(strand_, [this, self, buf]() {
        bool idle = send_queue_.empty();
        send_queue_.emplace(buf);
        if (idle) {
            AsyncWrite();
        }
    });
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