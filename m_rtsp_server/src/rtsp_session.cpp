#include "rtsp_session.h"
#include "rtsp_protocol.h"
#include "rtsp_log.h"
#include <sstream>
#include <algorithm>
#include <chrono>
namespace mx {
RtspSession::RtspSession(tcp::socket socket)
    : AsioTCPSession(std::move(socket)) {
    LOG_RTSP_INFO_AT("RtspSession::RtspSession created");
}

void RtspSession::OnBytes(const uint8_t* data, size_t size) {
    // LOG_RTSP_INFO_AT("Received data: {} bytes", size);
    // LOG_RTSP_INFO_AT("Data: {}", std::string(reinterpret_cast<const char*>(data), size));
    handleRtspRequest(std::string(reinterpret_cast<const char*>(data), size));
}

void RtspSession::OnClose() {
    LOG_RTSP_INFO_AT("RtspSession::OnClose called");
}

void RtspSession::handleRtspRequest(const std::string& request) {
    LOG_RTSP_INFO_AT("handleRtspRequest: {}", request);

    // 提取请求行
    std::istringstream iss(request);
    std::string request_line;
    std::getline(iss, request_line);

    // 解析请求行
    RtspMethod method;
    std::string url;
    std::string version;
    if (!RtspProtocol::ParseRtspRequestLine(request_line, method, url, version)) {
        LOG_RTSP_ERROR_AT("Invalid RTSP request line: {}", request_line);
        // 响应错误
        RtspResponse resp;
        resp.status_code = RtspStatus::BadRequest;
        resp.status_reason = "Bad Request";
        std::string cseq_header = "-1"; // 无效的CSeq
        // 解析请求CSeq            
        std::string response = buildResponse(resp, cseq_header);
        return;
    }

    // 解析请求头
    std::map<std::string, std::string> headers = RtspProtocol::ParseHeaders(request);
    // 更新CSeq
    if (headers.find("CSeq") != headers.end()) {
        try {
            context_.cseq = headers["CSeq"];
        }
        catch (const std::exception& e) {
            context_.cseq = "";
            LOG_RTSP_ERROR_AT("Invalid CSeq: {}", headers["CSeq"]);
        }
    }

    context_.url = url;

    std::string response;
    switch (method)
    {
    case RtspMethod::OPTIONS:
    {
        response = handleOptions(headers);
        break;
    }
    case RtspMethod::DESCRIBE:
    {
        response = handleDescribe(headers);
        break;
    }
    case RtspMethod::SETUP:
    {
        response = handleSetup(headers);
        break;
    }
    case RtspMethod::PLAY:
    {
        response = handlePlay(headers);
        break;
    }
    case RtspMethod::TEARDOWN:
    {
        response = handleTeardown(headers);
        break;
    }
    default:
        break;
    }

    LOG_RTSP_INFO_AT("Sending response: {}", response);
    Send(response);
}

std::string RtspSession::buildResponse(const RtspResponse& response, std::string cseq) {
    return RtspProtocol::BuildResponse(response, cseq);
}

std::string RtspSession::handleOptions(const std::map<std::string, std::string>& headers) {
    // 构建响应
    RtspResponse resp;
    resp.status_code = RtspStatus::OK;
    resp.status_reason = "OK";
    resp.headers["Public"] = "OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, PAUSE";
    return buildResponse(resp, context_.cseq);
}

std::string RtspSession::handleDescribe(const std::map<std::string, std::string>& headers) {
    // 构建响应
    RtspResponse resp;
    resp.status_code = RtspStatus::OK;
    resp.status_reason = "OK";
    // resp.headers["Content-Type"] = "application/sdp";    // build中已经设置

    std::string sdp = RtspProtocol::GenerateSdp(GetLocalAddress(), context_.session_id);
    // resp.headers["Content-Length"] = std::to_string(sdp.size());
    resp.body = sdp;
    return buildResponse(resp, context_.cseq);
}

std::string RtspSession::handleSetup(const std::map<std::string, std::string>& headers) {
    auto transport_it = headers.find("Transport");
    if (transport_it == headers.end()) {
        LOG_RTSP_ERROR_AT("Transport header not found in SETUP request");
        // 响应错误
        RtspResponse resp;
        resp.status_code = RtspStatus::BadRequest;
        resp.status_reason = "Bad Request";
        return buildResponse(resp, "-1");
    }
    std::string transport_header = transport_it->second;
    LOG_RTSP_INFO_AT("Transport header: {}", transport_header);

    if (context_.session_id.empty()) {
        context_.session_id = GetSessionID();
    }

    if (transport_header.find("RTP/AVP/TCP") != std::string::npos) {
        context_.is_tcp = true;
        // 解析interleaved参数
        std::size_t pos = transport_header.find("interleaved=");
        if (pos != std::string::npos) {
            std::string interleaved_str = transport_header.substr(pos + 12);
            std::size_t pos2 = interleaved_str.find("-");
            if (pos2 != std::string::npos) {
                context_.rtp_channel = std::stoi(interleaved_str.substr(0, pos2));                    
            }
            else {
                LOG_RTSP_ERROR_AT("Invalid interleaved parameter: {}", interleaved_str);
                context_.rtp_channel = 0;
                context_.rtcp_channel = 1;
            }
            std::size_t end = interleaved_str.find(";");
            if (end == std::string::npos) {
                // 可能是字符串末尾
                context_.rtcp_channel = std::stoi(interleaved_str.substr(pos2 + 1));
            } else {
                context_.rtcp_channel = std::stoi(interleaved_str.substr(pos2 + 1, end - pos2 - 1));
            }
        }
    } else if (transport_header.find("RTP/AVP") != std::string::npos && transport_header.find("unicast") != std::string::npos) { 
        // UDP 传输
        context_.is_tcp = false;
        // TODO
    }

    // 构建响应
    RtspResponse resp;
    resp.status_code = RtspStatus::OK;
    resp.status_reason = "OK";
    resp.headers["Session"] = context_.session_id;  // 会话ID
    if (context_.is_tcp) {
        resp.headers["Transport"] = "RTP/AVP/TCP;interleaved=" + std::to_string(context_.rtp_channel) + 
            "-" + std::to_string(context_.rtcp_channel);
    } else {
        // TODO
    }
    return buildResponse(resp, context_.cseq);
}

std::string RtspSession::handlePlay(const std::map<std::string, std::string>& headers) {
    if (context_.state != RtspState::READY) {
        RtspResponse resp;
        resp.status_code = RtspStatus::BadRequest;
        resp.status_reason = "Bad Request";
        return buildResponse(resp, context_.cseq);
    }
    context_.state = RtspState::PLAYING;
    //LOG_RTSP_INFO_AT("PLAY: {}" + context_.url);

    // RTP-Info 格式：url=<url>;seq=<sequence>;rtptime=<timestamp>
    std::string rtp_info = "url=" + context_.url + ";";
    if (context_.rtp_timestamp == 0) {
        // 使用当前时间戳作为初始时间戳
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        // 将毫秒转换为 90kHz 单位
        context_.rtp_timestamp = static_cast<uint32_t>((millis % 864000000) * 90 / 1000);
        // 随机生成初始序列号
        context_.rtp_seq_num = static_cast<uint16_t>(std::rand() % 65536);
    }

    rtp_info += "seq=" + std::to_string(context_.rtp_seq_num) + ";";
    rtp_info += "rtptime=" + std::to_string(context_.rtp_timestamp);

    // 构建响应
    RtspResponse resp;
    resp.status_code = RtspStatus::OK;
    resp.status_reason = "OK";
    resp.headers["Session"] = context_.session_id;  // 会话ID
    resp.headers["RTP-Info"] = rtp_info;
    return buildResponse(resp, context_.cseq);
}

std::string RtspSession::handleTeardown(const std::map<std::string, std::string>& headers) {
    context_.state = RtspState::TEARDOWN;
    
    // 构建响应
    RtspResponse resp;
    resp.status_code = RtspStatus::OK;
    resp.status_reason = "OK";
    resp.headers["Session"] = context_.session_id;  // 会话ID
    return buildResponse(resp, context_.cseq);
}

}
