#pragma once
#include <string>
#include <functional>
#include <boost/json.hpp>
#include <boost/asio.hpp>

namespace Json = boost::json;

class ZLMApiClient {
public:
    using ResponseCallback = std::function<void(bool success, const Json::object& response)>;

    struct Config {
        std::string host = "127.0.0.1";
        uint16_t port = 80;
        std::string secret;
    };

    explicit ZLMApiClient(boost::asio::io_context& io_ctx, const Config& cfg);

    void GetMediaList(ResponseCallback cb);
    void GetMediaList(const std::string& app, const std::string& stream, ResponseCallback cb);

    void GetMediaInfo(const std::string& app, const std::string& stream, ResponseCallback cb);

    void StartPull(const std::string& src_url,
                   const std::string& app,
                   const std::string& stream,
                   ResponseCallback cb);

    void StopPull(const std::string& app, const std::string& stream, ResponseCallback cb);

    void StartPush(const std::string& src_app,
                   const std::string& src_stream,
                   const std::string& dst_url,
                   ResponseCallback cb);

    void StopPush(const std::string& key, ResponseCallback cb);

    void CloseStream(const std::string& app, const std::string& stream, ResponseCallback cb);

    void GetServerConfig(ResponseCallback cb);

    void GetSnap(const std::string& app,
                 const std::string& stream,
                 const std::string& output_path,
                 ResponseCallback cb);

private:
    void DoRequest(const std::string& api, const Json::object& params, ResponseCallback cb);

    std::string BuildQuery(const Json::object& params);

private:
    boost::asio::io_context& io_context_;
    Config config_;
};
