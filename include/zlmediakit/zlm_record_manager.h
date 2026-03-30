#pragma once
#include <string>
#include <functional>
#include <boost/json.hpp>
#include <cstdint>
#include <boost/asio.hpp>

//namespace Json = boost::json;
//
//// ==================== 录制管理类 ====================
///**
// * @brief ZLMediaKit 录制管理模块
// * 
// * 功能：
// * - 录制控制（开始、停止、查询）
// * - 录制文件管理（列表、加载、删除）
// * - 录制播放控制（跳转、倍速）
// */
//class ZLMRecordManager {
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
//    explicit ZLMRecordManager(boost::asio::io_context& io_ctx, const Config& cfg);
//
//    // 录制控制
//    void StartRecord(const std::string& app,
//                     const std::string& stream,
//                     const std::string& type = "mp4",
//                     ResponseCallback cb = nullptr);
//    void StopRecord(const std::string& app,
//                    const std::string& stream,
//                    const std::string& type = "mp4",
//                    ResponseCallback cb = nullptr);
//    void IsRecording(const std::string& app, const std::string& stream, ResponseCallback cb = nullptr);
//
//    // 录制文件管理
//    void GetMP4RecordFile(const std::string& app,
//                          const std::string& stream,
//                          int64_t start_time,
//                          int64_t end_time,
//                          ResponseCallback cb = nullptr);
//    void LoadMP4File(const std::string& app,
//                     const std::string& stream,
//                     const std::string& file_path,
//                     bool is_audio = false,
//                     ResponseCallback cb = nullptr);
//    void DeleteRecordDirectory(const std::string& app,
//                               const std::string& stream,
//                               const std::string& period,
//                               ResponseCallback cb = nullptr);
//
//    // 录制播放控制
//    void SeekRecordStamp(const std::string& app,
//                         const std::string& stream,
//                         const std::string& type,
//                         int64_t stamp,
//                         ResponseCallback cb = nullptr);
//    void SetRecordSpeed(const std::string& app,
//                        const std::string& stream,
//                        double speed,
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
