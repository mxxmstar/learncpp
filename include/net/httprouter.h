#pragma once
#include <functional>
#include <boost/json.hpp>
#include <map>
#include <optional>
#include <string>
class HttpRouter {
public:
    struct RouteHandler {
        using Handler = std::function<void(const boost::json::object&, boost::json::object&)>;
        Handler handler;
        ///@brief 路由验证密钥，若有值则表示该路由为安全路由
		std::optional<std::string> api_key;
    };
    static HttpRouter& GetInstance();

    /// @brief 注册普通路由
    void RegisterRoute(const std::string& path, RouteHandler::Handler handler);
    /// @brief 注册安全路由
    void RegisterSecureRoute(const std::string& path, const std::string& api_key, RouteHandler::Handler handler);
	///@brief 分发请求（带签名）
    void DispatchRequest(const std::string& path, const std::string& body, const std::string& sign_header, boost::json::object& rsp);
    /// @brief 分发请求（不带签名）
    void DispatchRequest(const std::string& path, const boost::json::object& req, boost::json::object& rsp);

 private:
    HttpRouter() = default;
    std::map<std::string, RouteHandler> routes_;
    
};
