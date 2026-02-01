#include "zlmediakit/zlm_hookserver.h"
#include "log/logmanager.h"

ZLMHookHandler::ZLMHookHandler(const std::string& secret, EventHandler cb)
    : secret_(secret), cb_(std::move(cb))
{
}

bool ZLMHookHandler::HandleRequest(const http::request<http::string_body>& req, http::response<http::string_body>& rsp) {
    try { 
        // 解析请求
        auto req_obj = boost::json::parse(req.body()).as_object();

        // 验证密钥
        if (!req_obj.contains("secret")) {
            LOG_MAIN_ERROR_AT("Missing secret in zlm hook request");
            sendBadResponse(rsp);
            return false;
        }        
        if (req_obj["secret"].as_string().c_str() != secret_) {
            LOG_MAIN_ERROR_AT("Invalid secret in zlm hook request");
            sendForbiddenResponse(rsp);
            return false;
        }

        // 验证事件
        if (!req_obj.contains("event")) {
            LOG_MAIN_ERROR_AT("Missing event in zlm hook request");
            sendBadResponse(rsp);
            return false;
        }
        std::string event = req_obj["event"].as_string().c_str();
        boost::json::object rsp_obj;

        // 处理事件
        if (event == "on_publish") {

        } else if (event == "on_play") {

        } else {
            LOG_MAIN_ERROR_AT("Unknown event in zlm hook request", event);
            sendErrorResponse(rsp);
            return false;
        }

        rsp.body() = boost::json::serialize(rsp_obj);
        rsp.set(http::field::content_type, "application/json");
        rsp.result(http::status::ok);
        rsp.prepare_payload();
    } catch (const std::out_of_range& e) {
        LOG_MAIN_ERROR_AT("Missing required field in zlm hook request", e.what());
        sendBadResponse(rsp);
        return false;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to parse zlm hook request", e.what());
        sendErrorResponse(rsp);
        return false;
    }
}

void ZLMHookHandler::OnPublish(const boost::json::object& req, boost::json::object& rsp) {
    try { 
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("OnPlulish exception: {}", e.what());
        
    }
}

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

