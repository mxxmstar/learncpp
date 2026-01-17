#pragma once
#include "net/tcpsession.h"
#include "net/session.h"
#include <boost/asio.hpp>
#include <vector>
#include <memory>

using boost::asio::ip::tcp;
enum class ProtocolType {
    Unknown,
};

// class ProtocolDetector : public std::enable_shared_from_this<ProtocolDetector> {
// public:
//     enum class State {
//         Detecting,
//         Detected,
//         Detached,
//         Failed,
//     };
//     explicit ProtocolDetector(std::shared_ptr<Session> session);
//     ~ProtocolDetector() = default;
//     void OnBytes(const uint8_t* data, size_t size);
    

// private:
//     void TryDetect();
//     bool TryRTMP();
//     void AttachRTMP();
//     bool TryHTTP();
//     void AttachHTTP();
//     void OnDetectFailed();
    
//     std::shared_ptr<Session> session_;
//     std::vector<uint8_t> detect_data_;
//     State state_{ State::Detecting };
// };

