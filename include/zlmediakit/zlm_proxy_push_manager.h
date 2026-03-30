#pragma once
#include <string>
#include <functional>
#include <boost/json.hpp>
#include <boost/asio.hpp>

//namespace Json = boost::json;
//
//// ==================== 推流代理管理类 ====================
///**
// * @brief ZLMediaKit 推流代理管理模块
// * 
// * 功能：
// * - 添加/删除推流代理
// * - 查询推流代理信息
// */
//class ZLMProxyPushManager {
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
//    explicit ZLMProxyPushManager(boost::asio::io_context& io_ctx, const Config& cfg);
//
//    // 推流代理管理
//    void AddStreamPusherProxy(const std::string& src_url,
//                              const std::string& app,
//                              const std::string& stream,
//                              ResponseCallback cb = nullptr);
//    void DelStreamPusherProxy(const std::string& key, ResponseCallback cb = nullptr);
//    void GetProxyPusherInfo(const std::string& key, ResponseCallback cb = nullptr);
//
//private:
//    void DoRequest(const std::string& api, const Json::object& params, ResponseCallback cb);
//    std::string BuildQuery(const Json::object& params);
//
//private:
//    boost::asio::io_context& io_context_;
//    Config config_;
//};
