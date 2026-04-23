#include "net/http_server/http_router.h"
#include "log/logmanager.h"
#include <exception>
namespace Net {
static const std::vector<std::string> SIGNATURE_HEADERS = {
    "sign",
    "X-Sign"
};


HttpRouter& HttpRouter::GetInstance() {
    static HttpRouter instance;
    return instance;
}

void HttpRouter::RegisterRoute(const std::string& path, RouteHandler handler) {
    routes_[path] = std::move(handler);
}

void HttpRouter::RegisterModuleRoute(const std::string& module, ModuleHandler handler) {
    module_routes_[module] = std::move(handler);
}

void HttpRouter::DispatchRequest(const http::request<http::string_body>& req, boost::json::object& rsp) {
    // 先验签
    if (!getSignature(req).empty()) {
        if (validateSignature(req) == false) {
            rsp["code"] = 401;
            rsp["msg"] = "Authenticate signature failed.";
            return;
        }
    }

    std::string path = std::string(req.target());
    LOG_MAIN_INFO_AT("DispatchRequest path: {}", path);
    auto [module, module_path] = identifyModuleAndPath(path);

    // 模块路由
    if (!module.empty()) {
        auto it = module_routes_.find(module);
        if (it != module_routes_.end()) {
            LOG_MAIN_INFO_AT("DispatchRequest module: {}, path: {}", module, module_path);
            const auto& module_handler = it->second;

            try {
                boost::json::object req_obj;
                
                // 如果有请求体，则解析；否则使用空对象
                if (!req.body().empty()) {
                    boost::json::value jv = boost::json::parse(req.body());
                    if (!jv.is_object()) {
                        rsp["code"] = 400;
                        rsp["msg"] = "invalid request body.";
                        return;
                    }
                    req_obj = jv.as_object();
                }

                try {
                    module_handler(module_path, req_obj, rsp);
                }
                catch (std::exception& e) {
                    rsp["code"] = 500;
                    rsp["msg"] = std::string("exception: ") + e.what();
                }
            }
            catch (std::exception& e) {
                rsp["code"] = 500;
                rsp["msg"] = std::string("exception: ") + e.what();
            }
            return;
        }
    }

    // 普通路由
    auto it_router = routes_.find(path);
    if (it_router != routes_.end()) {
        const auto& route_handler = it_router->second;

        try {
            boost::json::object req_obj;
            
            // 如果有请求体，则解析；否则使用空对象
            if (!req.body().empty()) {
                boost::json::value jv = boost::json::parse(req.body());
                if (!jv.is_object()) {
                    rsp["code"] = 400;
                    rsp["msg"] = "invalid request body";
                    return;
                }
                req_obj = jv.as_object();
            }

            try {
                route_handler(req_obj, rsp);
            }
            catch (std::exception& e) {
                rsp["code"] = 500;
                rsp["msg"] = std::string("exception: ") + e.what();
            }

        }
        catch (std::exception& e) {
            rsp["code"] = 500;
            rsp["msg"] = std::string("exception: ") + e.what();
        }
    }
}

std::string HttpRouter::getSignature(const http::request<http::string_body>& req) {
    for (const auto& header : SIGNATURE_HEADERS) {
        auto it = req.find(header);
        if (it != req.end()) {
            return std::string(it->value());
        }
    }
    return "";
}

bool HttpRouter::validateSignature(const http::request<http::string_body>& req) {
    // TODO: 验签
    return true;
}

std::pair<std::string, std::string> HttpRouter::identifyModuleAndPath(const std::string& full_path) {
    for (const auto& [module, handler] : module_routes_) {
        if (full_path.starts_with("/" + module + "/") || full_path == "/" + module) {
            // 移除模块名和斜杠
            std::string module_path = full_path.substr(module.length() + 1);
            if (module_path.empty()) {
                module_path = "/";
            }
            else if (module_path[0] != '/') {
                module_path = "/" + module_path;
            }
            return { module, module_path };
        }
    }

    return { "", full_path };
}
}