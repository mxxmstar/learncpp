#include "rtsp/rtspsession.h"
#include "rtsp/rtsp.h"
#include "rtsp/rtspmacro.h"
#include "rtp/rtptransport.h"
#include "rtp/rtpsender.h"
#include "rtp/media.h"
#include <sstream>
#include <chrono>

namespace rtsp {

    RtspSession::RtspSession(boost::asio::ip::tcp::socket socket)
        : AsioTCPSession(std::move(socket)) {
        LOG_RTSP_INFO_AT("RtspSession created, session_id: {}", GetSessionID());
    }

    void RtspSession::SetMediaSources(const std::vector<std::shared_ptr<rtp::MediaSource>>& sources) {
        media_sources_ = sources;
    }

    void RtspSession::OnBytes(const uint8_t* data, size_t size) {
        // RTP/RTCP 数据包：$ + channel + length
        if (size >= 4 && data[0] == '$') {
            uint8_t channel = data[1];
            uint16_t length = (data[2] << 8) | data[3];

            if (channel == context_.rtcp_channel) {
                // RTCP 数据 - 可以选择忽略或记录
                HandleRtcpData(data + 4, length);
            }
            else if (channel == context_.rtp_channel) {
                // RTP 数据（如果有双向传输需求）
                HandleRtpData(data + 4, length);
            }
        }
        else {
            // RTSP 文本信令
            HandleRtspRequest(std::string(reinterpret_cast<const char*>(data), size));
        }
    }

    void RtspSession::OnClose() {
        LOG_RTSP_INFO_AT("RtspSession closed, session_id: {}", GetSessionID());
    }

    void RtspSession::HandleRtcpData(const uint8_t* data, size_t size) {
        // 处理 RTCP 数据
        // 可以选择记录日志或解析 RTCP 包
        LOG_RTSP_INFO_AT("Received RTCP data, size: {}", size);
    }

    void RtspSession::HandleRtpData(const uint8_t* data, size_t size) {
        // 处理 RTP 数据
        // 可以选择记录日志或解析 RTP 包
        LOG_RTSP_INFO_AT("Received RTP data, size: {}", size);
    }

    void RtspSession::HandleRtspRequest(const std::string& request) {
        LOG_RTSP_INFO_AT("handleRtspRequest: {}", request);
        std::istringstream iss(request);
        std::string request_line;
        std::getline(iss, request_line);

        RtspMethod method;
        std::string url;
        std::string version;
        if (!RtspProtocol::ParseRtspRequestLine(request_line, method, url, version)) {
            LOG_RTSP_ERROR_AT("Invalid RTSP request line: {}", request_line);
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::BadRequest);
            resp.status_reason = "Bad Request";
            std::string response = BuildResponse(resp, "-1");
            Send(response);
            return;
        }

        std::map<std::string, std::string> headers = RtspProtocol::ParseHeaders(request);

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
        switch (method) {
        case RtspMethod::OPTIONS:
            response = HandleOptions(headers);
            break;
        case RtspMethod::DESCRIBE:
            response = HandleDescribe(headers);
            break;
        case RtspMethod::SETUP:
            response = HandleSetup(headers);
            break;
        case RtspMethod::PLAY:
            response = HandlePlay(headers);
            break;
        case RtspMethod::TEARDOWN:
            response = HandleTeardown(headers);
            break;
        case RtspMethod::PAUSE:
            response = HandlePause(headers);
            break;
        default:
            break;
        }

        if (!response.empty()) {
            Send(response);
        }
    }

    std::string RtspSession::BuildResponse(const RtspResponse& response, const std::string& cseq) {
        return RtspProtocol::BuildResponse(response, cseq);
    }

    std::string RtspSession::HandleOptions(const std::map<std::string, std::string>& headers) {
        RtspResponse resp;
        resp.status_code = static_cast<int>(RtspStatus::OK);
        resp.status_reason = "OK";
        resp.headers["Public"] = "OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, PAUSE";
        return BuildResponse(resp, context_.cseq);
    }

    std::string RtspSession::HandleDescribe(const std::map<std::string, std::string>& headers) {
        RtspResponse resp;
        resp.status_code = static_cast<int>(RtspStatus::OK);
        resp.status_reason = "OK";

        if (context_.session_id.empty()) {
            context_.session_id = GetSessionID();
        }

        std::string sdp = RtspProtocol::GenerateSdp(GetLocalAddress(), context_.session_id, media_sources_);
        resp.body = sdp;
        return BuildResponse(resp, context_.cseq);
    }

    std::string RtspSession::HandleSetup(const std::map<std::string, std::string>& headers) {
        auto transport_it = headers.find("Transport");
        if (transport_it == headers.end()) {
            LOG_RTSP_ERROR_AT("Transport header not found in SETUP request");
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::BadRequest);
            resp.status_reason = "Bad Request";
            return BuildResponse(resp, context_.cseq);
        }

        std::string transport_header = transport_it->second;
        LOG_RTSP_INFO_AT("Transport header: {}", transport_header);

        // 解析client_port
        auto client_port_it = transport_header.find("client_port=");
        if (client_port_it == std::string::npos) {
            LOG_RTSP_ERROR_AT("client_port header not found in SETUP request");
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::BadRequest);
            resp.status_reason = "Bad Request";
            return BuildResponse(resp, context_.cseq);
        }
        auto client_port_pair = parseRangeNum(transport_header.substr(client_port_it + 12));
        if (client_port_pair.first.empty() || client_port_pair.second.empty()) {
            LOG_RTSP_ERROR_AT("Invalid client-Port range: {}", transport_header.substr(client_port_it + 12));
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::BadRequest);
            resp.status_reason = "Bad Request";
            return BuildResponse(resp, context_.cseq);
        }
        context_.client_rtp_port = std::stoi(client_port_pair.first);
        context_.client_rtcp_port = std::stoi(client_port_pair.second);

        if (context_.session_id.empty()) {
            context_.session_id = GetSessionID();
        }

        if (transport_header.find("RTP/AVP/TCP") != std::string::npos) {
            context_.mode = rtp::TransportMode::RTP_OVER_TCP;
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
                    context_.rtcp_channel = std::stoi(interleaved_str.substr(pos2 + 1));
                }
                else {
                    context_.rtcp_channel = std::stoi(interleaved_str.substr(pos2 + 1, end - pos2 - 1));
                }
            }
        }
        else if (transport_header.find("RTP/AVP") != std::string::npos &&
            transport_header.find("unicast") != std::string::npos) {
            context_.mode = rtp::TransportMode::RTP_OVER_UDP;
        }
        else if (transport_header.find("multicast") != std::string::npos) {
            context_.mode = rtp::TransportMode::RTP_OVER_MULTICAST;
            // TODO
        }
        else {
            LOG_RTSP_ERROR_AT("Invalid Transport header: {}", transport_header);
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::BadRequest);
            resp.status_reason = "Bad Request";
            return BuildResponse(resp, context_.cseq);
        }

        auto peer_endpoint = socket_.remote_endpoint();
        std::string peer_ip = peer_endpoint.address().to_string();
        uint16_t peer_port = peer_endpoint.port();

        context_.state = RtspState::READY;

        RtspResponse resp;
        resp.status_code = static_cast<int>(RtspStatus::OK);
        resp.status_reason = "OK";
        resp.headers["Session"] = context_.session_id;

        if (context_.mode == rtp::TransportMode::RTP_OVER_TCP) {
            auto sender = std::make_shared<rtp::AsioTcpRtpSender>(socket_);
            rtp_transport_ = std::make_shared<rtp::AsioRtpTransport>(GetIOContext());
            rtp_transport_->SetSender(sender);
            // 按照客户端的interleaved参数设置通道
            rtp_transport_->SetRtpOverTcp(rtp::MediaChannelId::channel_0,
                static_cast<uint16_t>(context_.rtp_channel),
                static_cast<uint16_t>(context_.rtcp_channel));
            rtp_transport_->SetClockRate(rtp::MediaChannelId::channel_0, 90000);
            rtp_transport_->SetPayloadType(rtp::MediaChannelId::channel_0, 96);
            rtp_transport_->SetSsrc(rtp::MediaChannelId::channel_0, static_cast<uint32_t>(std::rand()));
            rtp_transport_->Start();
            LOG_RTSP_INFO_AT("RTP over TCP transport created for channel {}", context_.rtp_channel);

            resp.headers["Transport"] = "RTP/AVP/TCP;interleaved=" +
                std::to_string(context_.rtp_channel) + "-" +
                std::to_string(context_.rtcp_channel);
        }
        else if (context_.mode == rtp::TransportMode::RTP_OVER_UDP) {
            auto sender = std::make_shared<rtp::AsioUdpRtpSender>(GetIOContext(), peer_ip,
                context_.client_rtp_port, context_.client_rtcp_port);
            rtp_transport_ = std::make_shared<rtp::AsioRtpTransport>(GetIOContext());
            rtp_transport_->SetSender(sender);
            rtp_transport_->SetRtpOverUdp(rtp::MediaChannelId::channel_0,
                context_.client_rtp_port,
                context_.client_rtcp_port);
            rtp_transport_->SetClockRate(rtp::MediaChannelId::channel_0, 90000);
            rtp_transport_->SetPayloadType(rtp::MediaChannelId::channel_0, 96);
            rtp_transport_->SetSsrc(rtp::MediaChannelId::channel_0, static_cast<uint32_t>(std::rand()));
            rtp_transport_->Start();
            LOG_RTSP_INFO_AT("RTP over UDP transport created");

            resp.headers["Transport"] = "RTP/AVP;unicast;client_port=" +
                std::to_string(context_.client_rtp_port) + "-" +
                std::to_string(context_.client_rtcp_port) + ";" +
                "server_port=" + std::to_string(sender->GetLocalPort()) + "-" +
                std::to_string(sender->GetLocalPort() + 1);
        }
        else if (context_.mode == rtp::TransportMode::RTP_OVER_MULTICAST) {
            // TODO
        }

        return BuildResponse(resp, context_.cseq);
    }

    std::string RtspSession::HandlePlay(const std::map<std::string, std::string>& headers) {
        if (context_.state != RtspState::READY) {
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::BadRequest);
            resp.status_reason = "Bad Request";
            return BuildResponse(resp, context_.cseq);
        }

        context_.state = RtspState::PLAYING;

        std::string rtp_info = "url=" + context_.url + ";";
        if (context_.rtp_timestamp == 0) {
            auto now = std::chrono::system_clock::now();
            auto epoch = now.time_since_epoch();
            auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
            context_.rtp_timestamp = static_cast<uint32_t>((millis % 864000000) * 90 / 1000);
            context_.rtp_seq_num = static_cast<uint16_t>(std::rand() % 65536);
        }

        rtp_info += "seq=" + std::to_string(context_.rtp_seq_num) + ";";
        rtp_info += "rtptime=" + std::to_string(context_.rtp_timestamp);

        RtspResponse resp;
        resp.status_code = static_cast<int>(RtspStatus::OK);
        resp.status_reason = "OK";
        resp.headers["Session"] = context_.session_id;
        resp.headers["RTP-Info"] = rtp_info;
        return BuildResponse(resp, context_.cseq);
    }

    std::string RtspSession::HandleTeardown(const std::map<std::string, std::string>& headers) {
        context_.state = RtspState::TEARDOWN;

        RtspResponse resp;
        resp.status_code = static_cast<int>(RtspStatus::OK);
        resp.status_reason = "OK";
        resp.headers["Session"] = context_.session_id;
        return BuildResponse(resp, context_.cseq);
    }

    std::string RtspSession::HandlePause(const std::map<std::string, std::string>& headers) {
        if (context_.state != RtspState::PLAYING) {
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::BadRequest);
            resp.status_reason = "Bad Request";
            return BuildResponse(resp, context_.cseq);
        }

        context_.state = RtspState::PAUSED;

        RtspResponse resp;
        resp.status_code = static_cast<int>(RtspStatus::OK);
        resp.status_reason = "OK";
        resp.headers["Session"] = context_.session_id;
        return BuildResponse(resp, context_.cseq);
    }

    std::pair<std::string, std::string> RtspSession::parseRangeNum(const std::string& str) {
        auto separator = str.find("-");
        if (separator == std::string::npos) {
            return std::pair{ "", "" };
        }

        // 第一部分
        auto start_part = str.substr(0, separator);

        auto end_part = str.substr(separator + 1);
        // 去除末尾的分隔符和空白等
        const char* delimiters = ";\r\n\t";
        const auto last_valid = end_part.find_last_not_of(delimiters);
        if (last_valid != std::string::npos) {
            end_part = end_part.substr(0, last_valid + 1);
        }
        else {
            end_part.clear();   // 没有有效字符，清空
        }
        return std::pair{ std::move(start_part), std::move(end_part) };
    }

    const std::string RtspSession::buildResponse(int status_code, const std::string& reason) {
        RtspResponse resp;
        resp.status_code = status_code;
        resp.status_reason = reason;
        resp.headers["Session"] = context_.session_id;
        return BuildResponse(resp, context_.cseq);
    }
}
