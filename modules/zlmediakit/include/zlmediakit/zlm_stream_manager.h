#pragma once
#include <string>
#include <functional>
#include <boost/json.hpp>
#include <boost/asio.hpp>

//namespace Json = boost::json;
//
//// ==================== 媒体流管理类 ====================
///**
// * @brief ZLMediaKit 媒体流管理模块
// * 
// * 功能：
// * - 媒体流查询（列表、信息、在线状态）
// * - 媒体流控制（关闭流）
// * - 会话管理（获取、踢出）
// */
//class ZLMStreamManager {
//public:
//    using ResponseCallback = std::function<void(bool success, const Json::object& response)>;
//
//    struct Config {
//        std::string host = "127.0.0.1";
//        uint16_t port = 80;
//        std::string secret;
//    };
//
//    // 包朋友，允许 ZLMApiClient 访问
//    friend class ZLMApiClient;
//
//private:
//    explicit ZLMStreamManager(boost::asio::io_context& io_ctx, const Config& cfg);
//
//    // 媒体流查询
//    void GetMediaList(ResponseCallback cb = nullptr);
//    void GetMediaList(const std::string& app, const std::string& stream, ResponseCallback cb = nullptr);
//    void GetMediaInfo(const std::string& app, const std::string& stream, ResponseCallback cb = nullptr);
//    void IsMediaOnline(const std::string& app, const std::string& stream, ResponseCallback cb = nullptr);
//    void GetMediaPlayerList(const std::string& app, const std::string& stream, ResponseCallback cb = nullptr);
//
//    // 媒体流控制
//    void CloseStream(const std::string& app, const std::string& stream, ResponseCallback cb = nullptr);
//    void CloseStreams(const std::string& app, ResponseCallback cb = nullptr);
//
//    // 会话管理
//    void GetAllSession(ResponseCallback cb = nullptr);
//    void GetAllSession(const std::string& app, const std::string& stream, ResponseCallback cb = nullptr);
//    void KickSession(const std::string& id, ResponseCallback cb = nullptr);
//    void KickSessions(const std::string& app, const std::string& stream, ResponseCallback cb = nullptr);
//
//private:
//    void DoRequest(const std::string& api, const Json::object& params, ResponseCallback cb);
//    std::string BuildQuery(const Json::object& params);
//
//private:
//    boost::asio::io_context& io_context_;
//    Config config_;
//};
