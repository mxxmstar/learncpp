#pragma once
#include <string>
#include <functional>
#include <boost/json.hpp>
#include <cstdint>
#include <boost/asio.hpp>

//namespace Json = boost::json;
//
//// ==================== RTP 服务管理类 ====================
///**
// * @brief ZLMediaKit RTP 服务管理模块
// * 
// * 功能：
// * - RTP 服务器管理（开启、列出、关闭）
// * - RTP 发送管理（主动、被动、停止）
// * - RTP 连接与检查
// */
//class ZLMRtpManager {
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
//    explicit ZLMRtpManager(boost::asio::io_context& io_ctx, const Config& cfg);
//
//    // RTP 服务器管理
//    void OpenRtpServer(const std::string& vhost,
//                       const std::string& app,
//                       const std::string& stream,
//                       uint16_t port,
//                       const std::string& tcp_mode = "0",
//                       ResponseCallback cb = nullptr);
//    void OpenRtpServerMultiplex(uint16_t port,
//                                const std::string& tcp_mode = "0",
//                                ResponseCallback cb = nullptr);
//    void ListRtpServer(ResponseCallback cb = nullptr);
//    void CloseRtpServer(const std::string& vhost,
//                        const std::string& app,
//                        const std::string& stream,
//                        ResponseCallback cb = nullptr);
//    void UpdateRtpServerSSRC(const std::string& vhost,
//                             const std::string& app,
//                             const std::string& stream,
//                             uint32_t ssrc,
//                             ResponseCallback cb = nullptr);
//
//    // RTP 发送管理
//    void StartSendRtp(const std::string& app,
//                      const std::string& stream,
//                      const std::string& dst_url,
//                      uint16_t dst_port,
//                      uint32_t ssrc,
//                      bool is_udp = true,
//                      ResponseCallback cb = nullptr);
//    void StartSendRtpPassive(const std::string& app,
//                             const std::string& stream,
//                             const std::string& ssrc,
//                             ResponseCallback cb = nullptr);
//    void StopSendRtp(const std::string& app,
//                     const std::string& stream,
//                     ResponseCallback cb = nullptr);
//    void ListRtpSender(ResponseCallback cb = nullptr);
//    void GetRtpInfo(const std::string& vhost,
//                    const std::string& app,
//                    const std::string& stream,
//                    ResponseCallback cb = nullptr);
//
//    // RTP 连接与检查
//    void ConnectRtpServer(const std::string& vhost,
//                          const std::string& app,
//                          const std::string& stream,
//                          const std::string& dst_url,
//                          uint16_t dst_port,
//                          ResponseCallback cb = nullptr);
//    void PauseRtpCheck(const std::string& vhost,
//                       const std::string& app,
//                       const std::string& stream,
//                       ResponseCallback cb = nullptr);
//    void ResumeRtpCheck(const std::string& vhost,
//                        const std::string& app,
//                        const std::string& stream,
//                        ResponseCallback cb = nullptr);
//
//private:
//    void DoRequest(const std::string& api, const Json::object& params, ResponseCallback cb);
//    std::string BuildQuery(const Json::object& params);
//
//private:
//    boost::asio::io_context& io_context_;
//    Config config_;
//};
