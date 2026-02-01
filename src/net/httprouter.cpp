#include "net/httprouter.h"
#include <exception>
#include <exception>
HttpRouter& HttpRouter::GetInstance() {
    static HttpRouter instance;
    return instance;
}

void HttpRouter::RegisterRoute(const std::string& path, RouteHandler::Handler handler) {
    routes_[path] = RouteHandler{std::move(handler), std::nullopt};
}

void HttpRouter::RegisterSecureRoute(const std::string& path, const std::string& api_key, RouteHandler::Handler handler) {
	routes_[path] = RouteHandler{ std::move(handler), api_key };
}

void HttpRouter::DispatchRequest(const std::string& path, const std::string& body, const std::string& sign_header, boost::json::object& rsp) {
    auto it = routes_.find(path);
    if (it == routes_.end()){
        rsp["code"] = 404;
        rsp["msg"] = "Not Found";
        return;
	}
    const auto& route_handler = it->second;
    if (route_handler.api_key.has_value()) {
        //TODO: 验证api_key
        if (body.empty()) {
            rsp["code"] = -1;
            rsp["msg"] = "missing raw body for sign check";
            return;
        }

        if (sign_header.empty()) {
            rsp["code"] = -1;
            rsp["msg"] = "missing sign";
            return;
        }
        /*std::string expected = hmac_sha256_hex(route.secret.value(), body);
        if (expected != sign_header) {
            rsp["code"] = -1;
            rsp["msg"] = "invalid sign";
            return;
        }*/
    }

    try {
        boost::json::value jv = boost::json::parse(body);
        if (!jv.is_object()) {
            rsp["code"] = 400;
            rsp["msg"] = "invalid request body";
            return;
        }
        const auto& req_obj = jv.as_object();
        route_handler.handler(req_obj, rsp);
    } catch (std::exception& e) {
        rsp["code"] = 500;
        rsp["msg"] = std::string("exception: ") + e.what();
	}
}

void HttpRouter::DispatchRequest(const std::string& path, const std::string& req, boost::json::object& rsp) { 
    auto it = routes_.find(path);
    if (it == routes_.end()) {
        rsp["code"] = 404;
        rsp["msg"] = "Not Found";
        return;
    }
    
	const auto& route_handler = it->second;
    
    try {
        boost::json::value jv = boost::json::parse(req);
        if (!jv.is_object()) {
            rsp["code"] = 400;
            rsp["msg"] = "invalid request body";
            return;
        }
        const auto& req_obj = jv.as_object();

        try {
            route_handler.handler(req_obj, rsp);
        } catch (std::exception& e) {
            rsp["code"] = 500;
            rsp["msg"] = std::string("exception: ") + e.what();
        }
        
    } catch (std::exception& e) {
        rsp["code"] = 500;
		rsp["msg"] = std::string("exception: ") + e.what();
    }
}