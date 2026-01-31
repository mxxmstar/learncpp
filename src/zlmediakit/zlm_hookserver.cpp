#include "zlmediakit/zlm_hookserver.h"


ZLMHookHandler::ZLMHookHandler(const std::string& secret, EventHandler cb)
	: secret_(secret), cb_(std::move(cb))
{
}

bool ZLMHookHandler::HandleRequest(const http::request<http::string_body>& req, http::response<http::string_body>& res)
{
	return false;
}

void ZLMHookHandler::sendForbiddenResponse(http::response<http::string_body>& res) {
	res.result(http::status::forbidden);
	res.set(http::field::content_type, "application/json");
	res.body() = R"({"code":403,"msg":"forbidden"})";
}

void ZLMHookHandler::sendBadResponse(http::response<http::string_body>& res) {
	res.result(http::status::bad_request);
	res.set(http::field::content_type, "application/json");
	res.body() = R"({"code":400,"msg":"bad request"})";
}

