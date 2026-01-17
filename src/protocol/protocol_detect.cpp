#include <string_view>

#include "protocol/protocol_detect.h"
#include "log/logmanager.h"
#include "protocol/protocolsession.h"

static constexpr size_t kMaxDetectDataSize = 4096;

// ProtocolDetector::ProtocolDetector(std::shared_ptr<Session> session)
//     : session_(session)
// {
// }

// void ProtocolDetector::OnBytes(const uint8_t* data, size_t size) {
//     if (state_ == State::Detached) {
//         return;
//     }
//     detect_data_.insert(detect_data_.end(), data, data + size);

//     if (detect_data_.size() >= kMaxDetectDataSize) {
//         OnDetectFailed();
//         return;
//     }
//     TryDetect();   // 继续等待数据探测
// }

// void ProtocolDetector::OnDetectFailed() {
//     LOG_MAIN_DEBUG_AT("ProtocolDetector::OnDetectFailed");
//     state_ = State::Failed;
//     session_->Stop();   // 停止当前会话
// }

// void ProtocolDetector::TryDetect() {
//     if (detect_data_.empty()) {
//         return;
//     }
//     if (TryRTMP()) {
//         LOG_MAIN_DEBUG_AT("Attach RTMP");
//         AttachRTMP();
//         return;
//     } else if (TryHTTP()) {
//         return;
//     }
// }

// void ProtocolDetector::Attach(std::shared_ptr<ProtocolSession> session) {
//     auto protocol_session = std::dynamic_pointer_cast<ProtocolSession>(session);
//     // 卸载 detector, 不再接收新的数据
//     session->SetDataHandler([this](const uint8_t* data, size_t size) {
//         OnBytes(data, size);
//     });
//     session->SetCloseHandler([this]() {
//         OnDetectFailed();
//     });
//     session->Start();
// }

// bool ProtocolDetector::TryRTMP() { 
//     // 尝试解析 RTMP 头
//     if (detect_data_.size() >= 1 && detect_data_[0] == 0x03) {
//         AttachRTMP();
//         return true;
//     }
//     return false;
// }

// void ProtocolDetector::AttachRTMP() { 
//     auto rtmp_session = std::make_shared<rtmp::RTMPSession>(session_);
//     Attach(rtmp_session);
// }

// // bool ProtocolDetector::TryHTTP() { 
// //     // 尝试解析 HTTP 头
// //     if (detect_data_.size() >= 4) {
// //         std::string_view s((char*)detect_data_.data(), detect_data_.size());
// //         if (s.starts_with("GET ") || s.starts_with("POST ")) {
// //             BindHTTP();
// //             return true;
// //         }
// //     }
// //     return false;
// // }

// // void ProtocolDetector::BindHTTP() { 
// //     auto http_session = std::make_shared<http::HTTPSession>(session_);
// // }




