#include "net/httpsession.h"
#include "log/logmanager.h"
AsioHttpSession::AsioHttpSession(tcp::socket&& socket)
    : socket_(std::move(socket)) {

}

AsioHttpSession::~AsioHttpSession() {
    
}

void AsioHttpSession::Start() {
    AsyncRead();
}

void AsioHttpSession::AsyncRead() {
    auto self(shared_from_this());
    http::async_read(socket_, buffer_, req_,
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            try { 
                if (ec) {
                    LOG_MAIN_ERROR_AT("AsyncRead error: {}, bytes_transferred: {}", ec.message(), bytes_transferred);
                    return;
                }
                self->HandleRequest();
                self->AsyncCheckDeadline();
            } catch (std::exception& e) {
                LOG_MAIN_ERROR_AT("AsyncRead error: {}", e.what());
            }        
        }
    );
}

void AsioHttpSession::AsyncWrite() {
    auto self(shared_from_this());
    rsp_.content_length(rsp_.body().size());
    http::async_write(socket_, rsp_,
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            try {
                if (ec) {
                    LOG_MAIN_ERROR_AT("AsyncWrite error: {}, bytes_transferred: {}", ec.message(), bytes_transferred);
                    return;
                }
                self->socket_.cancel(); // 取消所有待处理的异步操作
                self->deadline_timer_.cancel(); // 停止定时器
            } catch (std::exception& e) {
                LOG_MAIN_ERROR_AT("AsyncWrite error: {}", e.what());
            }
        }
    );
}

void AsioHttpSession::AsyncCheckDeadline() {
    auto self(shared_from_this());
    deadline_timer_.async_wait(
        [this, self](boost::system::error_code ec) {
            if (!ec) {
                // 超时处理
                LOG_MAIN_ERROR_AT("AsyncCheckDeadline timeout");
                socket_.close();
            }
        }
    );
}

void AsioHttpSession::HandleRequest() { 
    rsp_.version(req_.version());
    rsp_.keep_alive(false);
    rsp_.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    rsp_.set(boost::beast::http::field::access_control_allow_origin, "*");
    if (req_.method() == boost::beast::http::verb::get) {
        rsp_.result(boost::beast::http::status::ok);
        rsp_.body() = "Hello, World!";
        // TODO: 处理GET请求
        AsyncWrite();
    }

    if (req_.method() == boost::beast::http::verb::post) {
        rsp_.result(boost::beast::http::status::ok);
        rsp_.body() = "POST request received";
        // TODO: 处理POST请求
        AsyncWrite();
    }
}