#pragma once
#include <functional>
#include <boost/json.hpp>
#include <map>
#include <string>
#include <boost/beast/http.hpp>
#include <utility>
namespace beast = boost::beast;
namespace http = beast::http;

class HttpRouter {
public:    
    using RouteHandler = std::function<void(const boost::json::object&, boost::json::object&)>;
    using ModuleHandler = std::function<void(const std::string&, const boost::json::object&, boost::json::object&)>;
    static HttpRouter& GetInstance();

    /// @brief 注册普通路由
    void RegisterRoute(const std::string& path, RouteHandler handler);
    
    /// @brief 注册模块路由处理器
    void RegisterModuleRoute(const std::string& module, ModuleHandler handler);       

    /// @brief 分发请求
    void DispatchRequest(const http::request<http::string_body>& req, boost::json::object& rsp);
    
 private:
    HttpRouter() = default;

    /// @brief 获取请求签名
    std::string getSignature(const http::request<http::string_body>& req);

    bool validateSignature(const http::request<http::string_body>& req);
    /// @brief 获取并验证路由
    std::pair<std::string, std::string> identifyModuleAndPath(const std::string& full_path);

    /// @brief 路由处理器
    std::map<std::string, RouteHandler> routes_;

    /// @brief 模块处理器
    std::map<std::string, ModuleHandler> module_routes_;
    
};
