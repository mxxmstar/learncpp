#include "net/httprouter.h"

HttpRouter& HttpRouter::GetInstance() {
    static HttpRouter instance;
    return instance;
}

void HttpRouter::RegisterRoute(const std::string& path, Handler handler) {
    routes_[path] = handler;
}

void HttpRouter::DispatchRequest(const std::string& path, const boost::json::object& req, boost::json::object& rsp) { 
    auto it = routes_.find(path);
    if (it == routes_.end()) {
        rsp["code"] = 404;
        rsp["msg"] = "Not Found";
        return;
    }
    it->second(req, rsp);
}