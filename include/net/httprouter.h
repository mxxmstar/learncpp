#pragma once
#include <functional>
#include <boost/json.hpp>
#include <map>
class HttpRouter {
public:
    using Handler = std::function<void(const boost::json::object&, boost::json::object&)>;

    static HttpRouter& GetInstance();

    void RegisterRoute(const std::string& path, Handler handler);
    void DispatchRequest(const std::string& path, const boost::json::object& req, boost::json::object& rsp);

 private:
    HttpRouter() = default;
    std::map<std::string, Handler> routes_;
    
};
