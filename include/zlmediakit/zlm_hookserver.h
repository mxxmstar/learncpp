#pragma once
#include <memory>
#include <string>
#include <functional>
#include <boost/json.hpp>
#include <boost/beast.hpp>

namespace http = boost::beast::http;
/// @brief ZLMediaKit Hook 服务处理，用于接收 ZLMediaKit 的事件回调
/// 接收ZLM发送的 HTTP 请求; parse JSON; 抛事件
class ZLMHookHandler {
public:
    // TODO: 修改回调
    using EventHandler = std::function<void(
        const std::string& req,
        const boost::json::value& payload)>;

    explicit ZLMHookHandler(const std::string& secret, EventHandler cb);
    bool HandleRequest(
        const http::request<http::string_body>& req,
		http::response<http::string_body>& rsp);

    bool HandleJsonRequest(const boost::json::object& req_obj, boost::json::object& rsp_obj);
private:
    std::string secret_;
    EventHandler cb_;
    
    void OnStart(const boost::json::object& req, boost::json::object& rsp);
    void OnKeepalive(const boost::json::object& req, boost::json::object& rsp);
    
    void OnPublish(const boost::json::object& req, boost::json::object& rsp);
    void OnPlay(const boost::json::object& req, boost::json::object& rsp);


	void sendForbiddenResponse(http::response<http::string_body>& rsp);
	void sendBadResponse(http::response<http::string_body>& rsp);
    void sendErrorResponse(http::response<http::string_body>& rsp);
};


