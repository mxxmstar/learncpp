#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <functional>

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;
using IoContext = boost::asio::io_context;
constexpr int ASYNC_TIMEOUT_MS = 5000;
constexpr int SYNC_TIMEOUT_MS = 30000;

namespace Net {

class AsioSyncHttpClient {
public:
    explicit AsioSyncHttpClient(const std::string& host, uint16_t port);
    ~AsioSyncHttpClient();

    /// @brief 同步POST JSON请求
    /// @param url 请求URL
    /// @param req_obj 请求JSON对象
    /// @param rsp_obj 响应JSON对象
    /// @param timeout_ms 超时时间，单位毫秒
    /// @return 是否成功
    bool PostJson(const std::string& url, const boost::json::object& req_obj, boost::json::object& rsp_obj, int timeout_ms = SYNC_TIMEOUT_MS);

    /// @brief 同步GET请求
    /// @param url 请求URL
    /// @param rsp_obj 响应JSON对象
    /// @param timeout_ms 超时时间，单位毫秒
    /// @return 是否成功
    bool GetJson(const std::string& url, boost::json::object& rsp_obj, int timeout_ms = SYNC_TIMEOUT_MS);

    /// @brief 同步请求
    /// @param method 请求方法，如GET、POST等
    /// @param url 请求URL    
    /// @param req_obj 请求JSON对象
    /// @param rsp_obj 响应JSON对象
    /// @param timeout_ms 超时时间，单位毫秒
    /// @return 是否成功
    bool RequestJson(http::verb method, const std::string& url, const boost::json::object& req_obj, boost::json::object& rsp_obj, int timeout_ms = SYNC_TIMEOUT_MS);

private:
    std::string host_;
    uint16_t port_;
    
    /// @brief 连接到服务器
    bool connect(tcp::socket& socket, IoContext& ioc);
    /// @brief 异步发送请求
    /// @param socket 套接字
    /// @param method 请求方法，如GET、POST等
    /// @param url 请求URL
    /// @param req_obj 请求JSON对象
    /// @param rsp_obj 响应JSON对象
    /// @param timeout_ms 超时时间，单位毫秒
    /// @return 是否成功
    bool sendRequestJson(tcp::socket& socket, http::verb method, const std::string& url, const boost::json::object& req_obj, boost::json::object& rsp_obj, int timeout_ms);

};

class AsioAsyncHttpClient : public std::enable_shared_from_this<AsioAsyncHttpClient> {
public:
    using CompleteHandler = std::function<void(bool success, boost::json::object& rsp_obj)>;
    using TimeoutHandler = std::function<void()>;
    explicit AsioAsyncHttpClient(IoContext& ioc, const std::string& host, uint16_t port);
    ~AsioAsyncHttpClient();

    /// @brief 异步POST JSON请求
    /// @param url 请求URL
    /// @param req_obj 请求JSON对象
    /// @param handler 请求完成回调函数
    /// @param timeout_ms 超时时间，单位毫秒    
    void PostJson(const std::string& url, const boost::json::object& req_obj, CompleteHandler handler, int timeout_ms = ASYNC_TIMEOUT_MS);

    /// @brief 异步GET请求
    /// @param url 请求URL
    /// @param handler 响应JSON对象
    /// @param timeout_ms 超时时间，单位毫秒
    void GetJson(const std::string& url, CompleteHandler handler, int timeout_ms = ASYNC_TIMEOUT_MS);
private:
    struct RequestData {
        CompleteHandler handler;
        /// @brief 开始时间
        std::chrono::steady_clock::time_point start_time;
        /// @brief 超时时间
        std::chrono::milliseconds timeout_ms;
        http::request<http::string_body> req;
        http::response<http::string_body> rsp;
        beast::flat_buffer buffer;
        std::shared_ptr<tcp::socket> socket;
        std::shared_ptr<boost::asio::steady_timer> timer;
        std::atomic<bool> completed{ false };
    };

    void startResolve(std::shared_ptr<RequestData> req_data, const std::string& url);
    void handleResolve(std::shared_ptr<RequestData> req_data, beast::error_code ec, tcp::resolver::results_type results);
    void startConnect(std::shared_ptr<RequestData> req_data, tcp::resolver::results_type endpoints);
    void handleConnect(std::shared_ptr<RequestData> req_data, beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint);
    void startWrite(std::shared_ptr<RequestData> req_data);
    void handleWrite(std::shared_ptr<RequestData> req_data, beast::error_code ec, std::size_t bytes_transferred);
    void startRead(std::shared_ptr<RequestData> req_data);
    void handleRead(std::shared_ptr<RequestData> req_data, beast::error_code ec, std::size_t bytes_transferred);
    void startTimeout(std::shared_ptr<RequestData> req_data);
    void handleTimeout(std::shared_ptr<RequestData> req_data, beast::error_code ec);

    /// @brief 异步IO上下文
    IoContext& ioc_;
    /// @brief 解析主机名和端口号
    tcp::resolver resolver_;
    /// @brief 主机名
    std::string host_;
    /// @brief 端口号
    uint16_t port_;
};

}