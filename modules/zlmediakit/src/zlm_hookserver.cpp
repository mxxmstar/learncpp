#include "zlmediakit/zlm_hookserver.h"
#include "net/httprouter.h"
#include "log/logmanager.h"
using namespace Net;
ZLMHookHandler::ZLMHookHandler(const std::string& secret)
    : secret_(secret)
{
}

bool ZLMHookHandler::HandleRequest(const std::string& path, const boost::json::object& req_obj, boost::json::object& rsp_obj) {
    try { 
        if (path == "/hook/server_keepalive") {
            OnKeepalive(req_obj, rsp_obj);
        } else if (path == "/hook/server_started") {
            OnStart(req_obj, rsp_obj);
        } else if (path == "/hook/publish") {
            OnPublish(req_obj, rsp_obj);
        } else if (path == "/hook/play") {
            OnPlay(req_obj, rsp_obj);
        } else {
            rsp_obj["code"] = 404;
            rsp_obj["msg"] = "Not found";
            return false;
        }
        
    } catch (const std::out_of_range& e) {
        LOG_MAIN_ERROR_AT("Missing required field in zlm hook request: {}", e.what());
        rsp_obj["code"] = 400;
        rsp_obj["msg"] = "Missing required field";
        return false;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to parse zlm hook request: {}", e.what());
        rsp_obj["code"] = 500;
        rsp_obj["msg"] = "Internal server error";
        return false;
    }
    return true;
}

void ZLMHookHandler::RegisterRoutes() {
    auto& r = HttpRouter::GetInstance();
    r.RegisterModuleRoute("ZLMediaKit",
        [this](const std::string& path, const boost::json::object& req_obj, boost::json::object& rsp_obj) {
            this->HandleRequest(path, req_obj, rsp_obj);
        }
    );    
}

void ZLMHookHandler::OnStart(const boost::json::object& req, boost::json::object& rsp) {
    try {
        rsp["code"] = 200;
        rsp["msg"] = "server start";
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("OnStart exception: {}", e.what());
    }
}

void ZLMHookHandler::OnKeepalive(const boost::json::object& req, boost::json::object& rsp) {
    try { 
        rsp["code"] = 200;
        rsp["msg"] = "keepalive";
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("OnKeepalive exception: {}", e.what());
    }
}

void ZLMHookHandler::OnPublish(const boost::json::object& req, boost::json::object& rsp) {
    try { 
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("OnPlulish exception: {}", e.what());
        
    }
}

void ZLMHookHandler::OnPlay(const boost::json::object& req, boost::json::object& rsp) {
    try { 
        rsp["code"] = 200;
        rsp["msg"] = "play";
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("OnPlay exception: {}", e.what());
    }
}

///////////////TODO: result同意统一返回200, 错误码由ErrorCode.h定义
void ZLMHookHandler::sendForbiddenResponse(http::response<http::string_body>& rsp) {
    rsp.result(http::status::forbidden);
    rsp.set(http::field::content_type, "application/json");
    rsp.body() = R"({"code":403,"msg":"forbidden"})";
}

void ZLMHookHandler::sendBadResponse(http::response<http::string_body>& rsp) {
    rsp.result(http::status::bad_request);
    rsp.set(http::field::content_type, "application/json");
    rsp.body() = R"({"code":400,"msg":"bad request"})";
}

void ZLMHookHandler::sendErrorResponse(http::response<http::string_body>& rsp) {
    rsp.result(http::status::internal_server_error);
    rsp.set(http::field::content_type, "application/json");
    rsp.body() = R"({"code":500,"msg":"internal server error int json parse failed"})";
}

