#pragma once
#include <string>
#include <functional>
#include <boost/json.hpp>
#include <boost/asio.hpp>

//namespace Json = boost::json;
//
//// ==================== 系统管理类 ====================
///**
// * @brief ZLMediaKit 系统管理模块
// * 
// * 功能：
// * - 服务器配置（获取、设置）
// * - 服务器状态（版本、重启、统计）
// * - API 与消息（列表、广播）
// * - 文件下载
// * - WebRTC 会话与截图
// */
//class ZLMSystemManager {
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
//    explicit ZLMSystemManager(boost::asio::io_context& io_ctx, const Config& cfg);
//
//    // 服务器配置
//    void GetServerConfig(ResponseCallback cb = nullptr);
//    void SetServerConfig(const Json::object& config, ResponseCallback cb = nullptr);
//
//    // 服务器状态
//    void Version(ResponseCallback cb = nullptr);
//    void RestartServer(ResponseCallback cb = nullptr);
//    void GetStatistic(ResponseCallback cb = nullptr);
//    void GetThreadsLoad(ResponseCallback cb = nullptr);
//    void GetWorkThreadsLoad(ResponseCallback cb = nullptr);
//
//    // API 与消息
//    void GetApiList(ResponseCallback cb = nullptr);
//    void BroadcastMessage(const std::string& msg, ResponseCallback cb = nullptr);
//
//    // 文件下载
//    void DownloadFile(const std::string& file_path, ResponseCallback cb = nullptr);
//    void DownloadBin(ResponseCallback cb = nullptr);
//
//    // WebRTC 会话
//    void DeleteWebrtc(const std::string& app, const std::string& stream, ResponseCallback cb = nullptr);
//
//    // 截图
//    void GetSnap(const std::string& app,
//                 const std::string& stream,
//                 const std::string& output_path,
//                 ResponseCallback cb = nullptr);
//
//private:
//    void DoRequest(const std::string& api, const Json::object& params, ResponseCallback cb);
//    std::string BuildQuery(const Json::object& params);
//
//private:
//    boost::asio::io_context& io_context_;
//    Config config_;
//};
