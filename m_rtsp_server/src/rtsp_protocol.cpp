#include "rtsp_protocol.h"
#include "rtsp_log.h"
#include <sstream>
#include <algorithm>
namespace mx {

const std::string kRtspVersion = "RTSP/1.0";
const std::string kRtspServerName = "rtsp-server/1.0";

std::string RtspProtocol::BuildResponse(const RtspResponse& response, std::string cseq) {
    std::ostringstream oss;

    // 状态行 
    oss << kRtspVersion << " " << response.status_code << " " << response.status_reason << "\r\n";

    // 必要的头部字段
    oss << "CSeq: " << cseq << "\r\n"; 
    oss << "Server: " << kRtspServerName << "\r\n";
    
    // 添加其他头部字段
    for (const auto& header : response.headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }
    
    if (!response.body.empty()) {
        oss << "Content-Length: " << response.body.size() << "\r\n";
        oss << "Content-Type: " << response.content_type << "\r\n";
        oss << "\r\n" << response.body;
    } else {
        oss << "\r\n";
    }
    return oss.str();
}

    
bool RtspProtocol::ParseRequest(const std::string& request_str, RtspRequest& request) {
    // Parse request line
    // std::string request_line;
    // std::istringstream iss(request_str);
    // std::getline(iss, request_line);
    // if (!ParseRtspRequestLine(request_line, request.method, request.url, request.version)) {
    //     LOG_RTSP_ERROR("Failed to parse request line");
    // }
    // // 请求头
    // request.headers = std::move(ParseHeaders(request_str));
    
    // // 请求体
    return true;
}

bool RtspProtocol::ParseRtspRequestLine(const std::string& request_line, RtspMethod& method, std::string& url, std::string& version) {
    std::istringstream iss(request_line);
    std::string method_str;

    if (!(iss >> method_str >> url >> version)) {
        return false;
    }

    // 验证RTSP版本
    if (version != "RTSP/1.0") {
        return false;
    }

    // 转换为大写
    std::transform(method_str.begin(), method_str.end(), method_str.begin(), ::toupper);
    // 验证RTSP方法
    if (method_str == "OPTIONS") {
        method = RtspMethod::OPTIONS;
    } else if (method_str == "DESCRIBE") {
        method = RtspMethod::DESCRIBE;
    } else if (method_str == "SETUP") {
        method = RtspMethod::SETUP;
    } else if (method_str == "PLAY") {
        method = RtspMethod::PLAY;
    } else if (method_str == "PAUSE") {
        method = RtspMethod::PAUSE;
    } else if (method_str == "TEARDOWN") {
        method = RtspMethod::TEARDOWN;
    } else {
        method = RtspMethod::UNKNOWN;
        return false;
    }

    return true;
}

std::map<std::string, std::string> RtspProtocol::ParseHeaders(const std::string& request) {
    std::map<std::string, std::string> headers;
    std::istringstream iss(request);
    std::string line;
    
    // 跳过第一行（请求行）
    std::getline(iss, line);
    
    // 解析头部
    while (std::getline(iss, line) && !line.empty()) {
        // 移除\r字符
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // 去除前后空格
            key.erase(0, key.find_first_not_of(' '));
            key.erase(key.find_last_not_of(' ') + 1);
            value.erase(0, value.find_first_not_of(' '));
            value.erase(value.find_last_not_of(' ') + 1);
            
            headers[key] = value;
        }
    }
    
    return headers;
}

std::string RtspProtocol::GenerateSdp(const std::string ip, std::string& session_id) {
    std::ostringstream oss;

    // sdp 头
    oss << "v=0\r\n"
        << "o=- " << session_id << " 1234 " << "IN IP4 " << ip << "\r\n"
        << "s=RTSP Server\r\n"        
        << "c=IN IP4 0.0.0.0\r\n"
        << "t=0 0\r\n";

    // 音频描述
    oss << "m=audio 0 RTP/AVP 96\r\n"
        << "a=rtpmap:96 MPEG4-GENERIC/44100/2\r\n"        
        << "a=control:trackID=1\r\n";

    // 视频描述
    oss << "m=video 0 RTP/AVP 97\r\n"
        << "a=rtpmap:97 H264/90000\r\n"
        << "a=framerate:25\r\n"
        << "a=control:trackID=2\r\n"
        << "a=fmtp:97 packetization-mode=1\r\n";

    return oss.str();
}

}