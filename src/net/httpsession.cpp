#include "net/httpsession.h"
#include "log/logmanager.h"
#include "net/httprouter.h"

#include <boost/json.hpp>
#include <set>
AsioHttpSession::AsioHttpSession(tcp::socket&& socket)
    : IAsioSession(std::move(socket)) {

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
                /*self->HandleRequest();
                self->AsyncCheckDeadline();*/
                // 使用dynamic_pointer_cast转换为派生类指针
                auto http_self = std::dynamic_pointer_cast<AsioHttpSession>(self);
                if (http_self) {
                    http_self->HandleRequest();
                    http_self->AsyncCheckDeadline();
                }
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
                //self->socket_.cancel(); // 取消所有待处理的异步操作
                //self->deadline_timer_.cancel(); // 停止定时器
                auto http_self = std::dynamic_pointer_cast<AsioHttpSession>(self);
                http_self->socket_.cancel();
                http_self->deadline_timer_.cancel();
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
    // LOG_MAIN_INFO_AT("HandleRequest: {}", req_.target());
    
    // LOG_MAIN_INFO_AT("All headers:");
    // for (auto const& field : req_) {
    //     LOG_MAIN_INFO_AT("  {}: {}", field.name_string(), field.value());
    // }
    // LOG_MAIN_INFO_AT("Request Body: {}", req_.body());

    if (req_.method() == boost::beast::http::verb::get) {
        rsp_.result(boost::beast::http::status::ok);
        boost::json::object rsp_obj;
        rsp_obj["code"] = 0;
        rsp_obj["msg"] = "success";
        rsp_.body() = boost::json::serialize(rsp_obj);
        // TODO: 处理GET请求

        AsyncWrite();
    }

    if (req_.method() == boost::beast::http::verb::post) {                                                                       
        // 交给路由器 HttpRouter 处理
        boost::json::object rsp_obj;        
        HttpRouter::GetInstance().DispatchRequest(req_, rsp_obj);        
        std::string rsp_str = boost::json::serialize(rsp_obj);
        rsp_.body() = rsp_str;
        rsp_.result(boost::beast::http::status::ok);
        LOG_MAIN_INFO_AT("Response Body: {}", rsp_str);

        AsyncWrite();
    }
}

