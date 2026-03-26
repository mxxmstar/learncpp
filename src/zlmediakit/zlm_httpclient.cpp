#include "zlmediakit/zlm_httpclient.h"
#include "net/httpclientpool.h"
#include "log/logmanager.h"
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>
using namespace Net;
ZLMApiClient::ZLMApiClient(boost::asio::io_context& io_ctx, const Config& cfg)
    : io_context_(io_ctx), config_(cfg) {
}

void ZLMApiClient::GetMediaList(ResponseCallback cb) {
    Json::object params;
    DoRequest("getMediaList", params, std::move(cb));
}

void ZLMApiClient::GetMediaList(const std::string& app, const std::string& stream, ResponseCallback cb) {
    Json::object params;
    if (!app.empty()) params["app"] = app;
    if (!stream.empty()) params["stream"] = stream;
    DoRequest("getMediaList", params, std::move(cb));
}

void ZLMApiClient::GetMediaInfo(const std::string& app, const std::string& stream, ResponseCallback cb) {
    Json::object params;
    params["app"] = app;
    params["stream"] = stream;
    DoRequest("getMediaInfo", params, std::move(cb));
}

void ZLMApiClient::StartPull(const std::string& src_url,
                              const std::string& app,
                              const std::string& stream,
                              ResponseCallback cb) {
    Json::object params;
    params["src_url"] = src_url;
    params["app"] = app;
    params["stream"] = stream;
    DoRequest("startPull", params, std::move(cb));
}

void ZLMApiClient::StopPull(const std::string& app, const std::string& stream, ResponseCallback cb) {
    Json::object params;
    params["app"] = app;
    params["stream"] = stream;
    DoRequest("stopPull", params, std::move(cb));
}

void ZLMApiClient::StartPush(const std::string& src_app,
                              const std::string& src_stream,
                              const std::string& dst_url,
                              ResponseCallback cb) {
    Json::object params;
    params["app"] = src_app;
    params["stream"] = src_stream;
    params["dst_url"] = dst_url;
    DoRequest("startPush", params, std::move(cb));
}

void ZLMApiClient::StopPush(const std::string& key, ResponseCallback cb) {
    Json::object params;
    params["key"] = key;
    DoRequest("stopPush", params, std::move(cb));
}

void ZLMApiClient::CloseStream(const std::string& app, const std::string& stream, ResponseCallback cb) {
    Json::object params;
    params["app"] = app;
    params["stream"] = stream;
    params["force"] = 1;
    DoRequest("close_stream", params, std::move(cb));
}

void ZLMApiClient::GetServerConfig(ResponseCallback cb) {
    Json::object params;
    DoRequest("getServerConfig", params, std::move(cb));
}

void ZLMApiClient::GetSnap(const std::string& app,
                            const std::string& stream,
                            const std::string& output_path,
                            ResponseCallback cb) {
    Json::object params;
    params["app"] = app;
    params["stream"] = stream;
    params["snap_path"] = output_path;
    DoRequest("getSnap", params, std::move(cb));
}

void ZLMApiClient::DoRequest(const std::string& api, const Json::object& params, ResponseCallback cb) {
    auto& pool = HttpClientPool::GetInstance();

    auto client = pool.Acquire();
    if (!client) {
        LOG_MAIN_ERROR_AT("ZLMApiClient: failed to acquire HTTP client");
        if (cb) cb(false, {{"code", -1}, {"msg", "no available client"}});
        return;
    }

    Json::object full_params = params;
    full_params["secret"] = config_.secret;

    std::string query = BuildQuery(full_params);
    std::string url = "/index/api/" + api + "?" + query;

    client->GetJson(url, [cb = std::move(cb)](bool success, const Json::object& rsp) {
        if (!success) {
            if (cb) cb(false, {{"code", -1}, {"msg", "http request failed"}});
            return;
        }

        if (rsp.contains("code") && rsp.at("code").is_number()) {
            int code = rsp.at("code").as_int64();
            if (code == 0) {
                if (cb) cb(true, rsp);
            } else {
                std::string msg = rsp.contains("msg") && rsp.at("msg").is_string()
                    ? std::string(rsp.at("msg").as_string())
                    : "unknown error";
                if (cb) cb(false, {{"code", code}, {"msg", msg}});
            }
        } else {
            if (cb) cb(false, {{"code", -1}, {"msg", "invalid response"}});
        }
    });
}

std::string ZLMApiClient::BuildQuery(const Json::object& params) {
    std::ostringstream oss;
    bool first = true;

    for (const auto& [key, value] : params) {
        if (!first) oss << "&";
        first = false;

        oss << key << "=";
        if (value.is_string()) {
            oss << std::string(value.as_string());
        } else if (value.is_int64()) {
            oss << value.as_int64();
        } else if (value.is_uint64()) {
            oss << value.as_uint64();
        } else if (value.is_double()) {
            oss << value.as_double();
        } else if (value.is_bool()) {
            oss << (value.as_bool() ? "1" : "0");
        }
    }

    return oss.str();
}
