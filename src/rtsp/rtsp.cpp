#include "rtsp/rtsp.h"
#include "rtsp/rtspmacro.h"
#include <sstream>
#include <algorithm>

namespace rtsp {

const std::string kRtspVersion = "RTSP/1.0";
const std::string kRtspServerName = "RTSPServer/1.0";

std::string RtspProtocol::BuildResponse(const RtspResponse& response, const std::string& cseq) {
    std::ostringstream oss;

    oss << kRtspVersion << " " << response.status_code << " " << response.status_reason << "\r\n";
    oss << "CSeq: " << cseq << "\r\n";
    oss << "Server: " << kRtspServerName << "\r\n";

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
    return true;
}

bool RtspProtocol::ParseRtspRequestLine(const std::string& request_line, 
                                         RtspMethod& method, 
                                         std::string& url, 
                                         std::string& version) {
    std::istringstream iss(request_line);
    std::string method_str;

    if (!(iss >> method_str >> url >> version)) {
        return false;
    }

    if (version != "RTSP/1.0") {
        return false;
    }

    std::transform(method_str.begin(), method_str.end(), method_str.begin(), ::toupper);

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
    } else if (method_str == "GET_PARAMETER") {
        method = RtspMethod::GET_PARAMETER;
    } else if (method_str == "SET_PARAMETER") {
        method = RtspMethod::SET_PARAMETER;
    } else if (method_str == "RECORD") {
        method = RtspMethod::RECORD;
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

    std::getline(iss, line);

    while (std::getline(iss, line) && !line.empty()) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            key.erase(0, key.find_first_not_of(' '));
            key.erase(key.find_last_not_of(' ') + 1);
            value.erase(0, value.find_first_not_of(' '));
            value.erase(value.find_last_not_of(' ') + 1);

            headers[key] = value;
        }
    }

    return headers;
}

std::string RtspProtocol::GenerateSdp(const std::string& ip, 
                                       const std::string& session_id,
                                       const std::vector<std::shared_ptr<rtp::MediaSource>>& sources) {
    std::ostringstream oss;

    oss << "v=0\r\n"
        << "o=- " << session_id << " 1234 IN IP4 " << ip << "\r\n"
        << "s=RTSP Server\r\n"
        << "c=IN IP4 0.0.0.0\r\n"
        << "t=0 0\r\n";

    for (size_t i = 0; i < sources.size(); ++i) {
        if (sources[i] != nullptr) {
            oss << sources[i]->GetMediaDescription(0) << "\r\n";
            oss << sources[i]->GetAttribute() << "\r\n";
            oss << "a=control:track" << i << "\r\n";
        }
    }

    return oss.str();
}

}
