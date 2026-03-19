#pragma once
#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include "rtp/mediasource.h"

namespace rtsp {

enum class RtspMethod {
    OPTIONS,
    DESCRIBE,
    SETUP,
    PLAY,
    PAUSE,
    TEARDOWN,
    GET_PARAMETER,
    SET_PARAMETER,
    RECORD,
    UNKNOWN
};

enum class RtspState {
    INIT,
    READY,
    PLAYING,
    PAUSED,
    TEARDOWN
};

enum class RtspStatus {
    OK = 200,
    BadRequest = 400,
    NotFound = 404,
    MethodNotAllowed = 405,
    SessionNotFound = 454,
    InternalServerError = 500
};

struct RtspRequest {
    RtspMethod method = RtspMethod::UNKNOWN;
    std::string url;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct RtspResponse {
    int status_code = 200;
    std::string status_reason = "OK";
    std::map<std::string, std::string> headers {};
    std::string body {};
    std::string content_type = "application/sdp";
};

struct RtspContext {
    std::string session_id;
    RtspState state = RtspState::INIT;
    std::string cseq;   // 客户端请求的CSeq，用于匹配响应
    std::string url;    // 请求的URL
    uint16_t client_rtp_port = 0;
    uint16_t client_rtcp_port = 0;
    uint32_t rtp_timestamp = 0;
    uint16_t rtp_seq_num = 0;
    rtp::TransportMode mode = rtp::TransportMode::RTP_OVER_TCP;
    int rtp_channel = 0;
    int rtcp_channel = 1;
};

class RtspProtocol {
public:
    static std::string BuildResponse(const RtspResponse& response, const std::string& cseq);

    static bool ParseRequest(const std::string& request_str, RtspRequest& request);

    static bool ParseRtspRequestLine(const std::string& request_line, 
                                      RtspMethod& method, 
                                      std::string& url, 
                                      std::string& version);

    static std::map<std::string, std::string> ParseHeaders(const std::string& request);

    static std::string GenerateSdp(const std::string& ip, 
                                    const std::string& session_id,
                                    const std::vector<std::shared_ptr<rtp::MediaSource>>& sources);
};

}
