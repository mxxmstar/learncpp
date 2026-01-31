#pragma once
#include <string>
#include <boost/json.hpp>

namespace Json = boost::json;

class ZLMApiClient {
public:
    struct Config {
        std::string host;
        int port;
        std::string api_secret;
    };

    explicit ZLMApiClient(const Config& cfg);

    Json::value getStreamList();

    bool startPull(const std::string& src,
                   const std::string& app,
                   const std::string& stream);

    bool startPush(const std::string& src,
                   const std::string& dst);

    Json::value getStreamStatus(const std::string& app,
                                const std::string& stream);

private:
    Json::value httpGet(const std::string& path);
    Json::value httpPost(const std::string& path,
                         const Json::value& body);

private:
    Config config_;
};
