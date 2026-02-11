#include "net/httpclient.h"
#include "log/logmanager.h"
#include "common/errcode.h"
#include <ctime>
#include <thread>

namespace HttpErrCode =  ErrorCode::Net::Http;

AsioSyncHttpClient::AsioSyncHttpClient(const std::string& host, uint16_t port) : host_(host), port_(port)
{
}

AsioSyncHttpClient::~AsioSyncHttpClient()
{
}

bool AsioSyncHttpClient::PostJson(const std::string& url, const boost::json::object& req_obj, boost::json::object& rsp_obj, int timeout_ms) {
    return RequestJson(http::verb::post, url, req_obj, rsp_obj, timeout_ms);
}

bool AsioSyncHttpClient::GetJson(const std::string& url, boost::json::object& rsp_obj, int timeout_ms) {
    return RequestJson(http::verb::get, url, {}, rsp_obj, timeout_ms);
}

bool AsioSyncHttpClient::RequestJson(http::verb method, const std::string& url, const boost::json::object& req_obj, boost::json::object& rsp_obj, int timeout_ms) {
    try {
        // 创建 TCP socket
        net::io_context io_ctx;
        tcp::socket socket(io_ctx);
        if (!connect(socket, io_ctx)) {
            LOG_MAIN_ERROR_AT("Failed to connect to {}:{} for url {}", host_, port_, url);
            return false;
        }

        return sendRequestJson(socket, method, url, req_obj, rsp_obj, timeout_ms);
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Exception in HTTP request to {}:{}, url {}: {}", host_, port_, url, e.what());
        return false;
    }
}

bool AsioSyncHttpClient::connect(tcp::socket& socket, net::io_context& ioc) {
    tcp::resolver resolver(ioc);
    // 解析主机名和端口
    auto const results = resolver.resolve(host_, std::to_string(port_));
    // 连接到服务器
    net::connect(socket, results);
    return true;
}

bool AsioSyncHttpClient::sendRequestJson(tcp::socket& socket, http::verb method, const std::string& url, const boost::json::object& req_obj, boost::json::object& rsp_obj, int timeout_ms) { 
    try { 
        // 序列化请求体
        std::string req_body = req_obj.empty() ? "" : boost::json::serialize(req_obj);

        // 构建HTTP请求
        http::request<http::string_body> req{ method, url, 11 };
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::content_type, "application/json");
        if (!req_body.empty()) {
            req.body() = req_body;
            req.prepare_payload();
        }

        // 发送HTTP请求
        http::write(socket, req);

        // 创建接收缓冲区
        boost::beast::flat_buffer buffer;
        // 接收HTTP响应
        http::response<http::string_body> rsp;
        http::read(socket, buffer, rsp);

        // 解析响应体
        if (rsp.result() != http::status::ok && rsp.result() != http::status::forbidden
            && rsp.result() != http::status::not_found) {
            
            LOG_MAIN_WARN_AT("HTTP request to {}:{} returned status code: {}", 
                            host_, port_, rsp.result_int());            
        }

        try {
            // 解析响应体为JSON对象
            if (rsp.body().size() > 0) {
                boost::json::value rsp_value = boost::json::parse(rsp.body());
                if (rsp_value.is_object()) {
                    rsp_obj = rsp_value.as_object();
                    return true;
                } else {
                    LOG_MAIN_ERROR_AT("HTTP {}:{} response body is not a JSON object", host_, port_);
                    return false;
                }
            } else {
                rsp_obj = {};
                return true;
            }
        }
        catch (const std::exception& e) {
            LOG_MAIN_ERROR_AT("Failed to parse HTTP response body: {}", e.what());
            return false;
        }
    } catch (const std::exception& e) { 
        LOG_MAIN_ERROR_AT("Exception in HTTP request to {}:{}: {}", host_, port_, e.what());
        return false;
    }
}


AsioAsyncHttpClient::AsioAsyncHttpClient(net::io_context& ioc, const std::string& host, uint16_t port) : ioc_(ioc), resolver_(ioc), host_(host), port_(port)
{
}

AsioAsyncHttpClient::~AsioAsyncHttpClient()
{
}

void AsioAsyncHttpClient::PostJson(const std::string& url, const boost::json::object& req_obj, CompleteHandler handler, int timeout_ms) {
    auto req_data = std::make_shared<RequestData>();
    req_data->handler = handler;
    // 记录开始时间
    req_data->start_time = std::chrono::steady_clock::now();
    // 设置超时时间
    req_data->timeout_ms = std::chrono::milliseconds(timeout_ms);
    req_data->socket = std::make_shared<tcp::socket>(ioc_);
    req_data->timer = std::make_shared<net::steady_timer>(ioc_);

    // 构建HTTP请求
    std::string req_body = boost::json::serialize(req_obj);
    req_data->req.method(http::verb::post);
    req_data->req.target(url);
    req_data->req.version(11);
    req_data->req.set(http::field::host, host_);
    req_data->req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req_data->req.set(http::field::content_type, "application/json");
    req_data->req.set(http::field::content_length, std::to_string(req_body.size()));
    req_data->req.body() = req_body;
    req_data->req.prepare_payload();

    startResolve(req_data, url);
}

void AsioAsyncHttpClient::GetJson(const std::string& url, CompleteHandler handler, int timeout_ms) {
    auto req_data = std::make_shared<RequestData>();
    req_data->handler = handler;
    // 记录开始时间
    req_data->start_time = std::chrono::steady_clock::now();
    // 设置超时时间
    req_data->timeout_ms = std::chrono::milliseconds(timeout_ms);
    req_data->socket = std::make_shared<tcp::socket>(ioc_);
    req_data->timer = std::make_shared<net::steady_timer>(ioc_);

    // 构建HTTP请求
    req_data->req.method(http::verb::get);
    req_data->req.target(url);
    req_data->req.version(11);
    req_data->req.set(http::field::host, host_);
    req_data->req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req_data->req.prepare_payload();

    startResolve(req_data, url);
}

void AsioAsyncHttpClient::startResolve(std::shared_ptr<RequestData> req_data, const std::string& url) {
    // 开始超时定时器
    startTimeout(req_data);
    auto self = shared_from_this();
    resolver_.async_resolve(
        host_, std::to_string(port_),
        [self, req_data](beast::error_code ec, tcp::resolver::results_type results) {
            self->handleResolve(req_data, ec, results);
        }
    );
}

void AsioAsyncHttpClient::handleResolve(std::shared_ptr<RequestData> req_data, beast::error_code ec, tcp::resolver::results_type results) {     
    if (ec) {
        // 取消超时定时器
        req_data->timer->cancel();

        LOG_MAIN_ERROR_AT("Failed to resolve url {}:{}", host_, port_);
        boost::json::object rsp_obj;
        rsp_obj["code"] = ec.value();
        rsp_obj["msg"] = ec.message();
        req_data->handler(false, rsp_obj);
        req_data->completed = true;
        return;
    }
    
    startConnect(req_data, results);
}

void AsioAsyncHttpClient::startConnect(std::shared_ptr<RequestData> req_data, tcp::resolver::results_type endpoints) {    
    auto self = shared_from_this();    
    net::async_connect(
        *req_data->socket,
        endpoints,        
        [self, req_data](beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint) {
            self->handleConnect(req_data, ec, endpoint);
        }
    );    
}

void AsioAsyncHttpClient::handleConnect(std::shared_ptr<RequestData> req_data, beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint) {
    if (ec) {
        // 取消超时定时器
        req_data->timer->cancel();

        LOG_MAIN_ERROR_AT("Failed to connect to url {}:{}", host_, port_);
        boost::json::object rsp_obj;
        rsp_obj["code"] = ec.value();
        rsp_obj["msg"] = ec.message();
        req_data->handler(false, rsp_obj);
        req_data->completed = true;
        return;
    }
    
    startWrite(req_data);
}

void AsioAsyncHttpClient::startWrite(std::shared_ptr<RequestData> req_data) {    
    auto self = shared_from_this();
    http::async_write(
        *req_data->socket, req_data->req,
        [self, req_data](beast::error_code ec, std::size_t bytes_transferred) {
            self->handleWrite(req_data, ec, bytes_transferred);
        }
    );
}

void AsioAsyncHttpClient::handleWrite(std::shared_ptr<RequestData> req_data, beast::error_code ec, std::size_t bytes_transferred) { 
    if (ec) {
        // 取消超时定时器
        req_data->timer->cancel();

        LOG_MAIN_ERROR_AT("Failed to write request to url {}:{}", host_, port_);
        boost::json::object rsp_obj;
        rsp_obj["code"] = ec.value();
        rsp_obj["msg"] = ec.message();
        req_data->handler(false, rsp_obj);
        req_data->completed = true;
        return;
    }

    startRead(req_data);
}

void AsioAsyncHttpClient::startRead(std::shared_ptr<RequestData> req_data) {     
    auto self = shared_from_this();    
    http::async_read(
        *req_data->socket,req_data->buffer, req_data->rsp,
        [self, req_data](beast::error_code ec, std::size_t bytes_transferred) {
            self->handleRead(req_data, ec, bytes_transferred);
        }
    );
}

void AsioAsyncHttpClient::handleRead(std::shared_ptr<RequestData> req_data, beast::error_code ec, std::size_t bytes_transferred) { 
    if (ec) {
        // 取消超时定时器
        req_data->timer->cancel();

        LOG_MAIN_ERROR_AT("Failed to read response from url {}:{}", host_, port_);
        boost::json::object rsp_obj;
        rsp_obj["code"] = ec.value();
        rsp_obj["msg"] = ec.message();
        req_data->handler(false, rsp_obj);
        req_data->completed = true;
        return;
    }
    // 接收成功，取消接收响应阶段的超时定时器，请求完成
    req_data->timer->cancel();
    // 解析响应体
    try {
        boost::json::object rsp_obj;
        if (req_data->rsp.body().size() > 0) {
            boost::json::value rsp_value = boost::json::parse(req_data->rsp.body());
            if (rsp_value.is_object()) {
                rsp_obj = rsp_value.as_object();
                req_data->handler(true, rsp_obj);
            }
            else {
                LOG_MAIN_ERROR_AT("HTTP {}:{} response body is not a json object", host_, port_);
                rsp_obj["code"] = -1;
                rsp_obj["msg"] = "response body is not a json object";
                req_data->handler(false, rsp_obj);
            }
        }
        else {
            // TODO
            req_data->handler(true, rsp_obj);
        }
        // 请求完成
        req_data->completed = true;
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to parse HTTP response body: {}", e.what());
        boost::json::object rsp_obj;
        rsp_obj["code"] = -1;
        rsp_obj["msg"] = std::string("failed to parse response: ") + e.what();
        req_data->handler(false, rsp_obj);
    }
}

void AsioAsyncHttpClient::startTimeout(std::shared_ptr<RequestData> req_data) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - req_data->start_time);
    auto remaining = req_data->timeout_ms - elapsed;
    
    if (remaining.count() <= 0) {
        handleTimeout(req_data, {});
        return;
    }

    req_data->timer->expires_after(remaining);
    auto self = weak_from_this();
    req_data->timer->async_wait([self, req_data](beast::error_code ec) {
        if (auto ptr = self.lock()) {
            ptr->handleTimeout(req_data, ec);
        }        
    });
}

void AsioAsyncHttpClient::handleTimeout(std::shared_ptr<RequestData> req_data, beast::error_code ec) {
    if (ec == net::error::operation_aborted) {
        // 定时器已被取消
        return;
    }

    // 关闭连接
    req_data->socket->close();

    // 返回超时错误
    boost::json::object rsp_obj;
    rsp_obj["code"] = 408;  // Request Timeout
    rsp_obj["msg"] = "Request timeout";
    req_data->handler(false, rsp_obj);
}