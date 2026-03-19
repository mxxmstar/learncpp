#pragma once
#include <memory>
#include <vector>
#include <utility>
#include "net/tcpsession.h"
#include "rtsp/rtsp.h"
#include "rtp/rtptransport.h"
#include "rtp/rtpsender.h"
#include "rtp/media.h"

namespace rtsp {

class RtspSession : public AsioTCPSession {
public:
    explicit RtspSession(boost::asio::ip::tcp::socket socket);

    void SetMediaSources(const std::vector<std::shared_ptr<rtp::MediaSource>>& sources);

protected:
    void OnBytes(const uint8_t* data, size_t size) override;
    void OnClose() override;

private:
    void HandleRtcpData(const uint8_t* data, size_t size);
    void HandleRtpData(const uint8_t* data, size_t size);
    
    void HandleRtspRequest(const std::string& request);
    std::string BuildResponse(const RtspResponse& response, const std::string& cseq);

    std::string HandleOptions(const std::map<std::string, std::string>& headers);
    std::string HandleDescribe(const std::map<std::string, std::string>& headers);
    std::string HandleSetup(const std::map<std::string, std::string>& headers);
    std::string HandlePlay(const std::map<std::string, std::string>& headers);
    std::string HandleTeardown(const std::map<std::string, std::string>& headers);
    std::string HandlePause(const std::map<std::string, std::string>& headers);

    /// @brief 解析Range头中的数字范围
    /// @param str Range头中的字符串，例如"npt=10.0-20.0"
    /// @return 包含范围开始和结束的std::pair，例如{"10.0", "20.0"}
    inline std::pair<std::string, std::string> parseRangeNum(const std::string& str);
    inline const std::string buildResponse(int status_code, const std::string& reason);
    ///@brief RTSP 上下文
    RtspContext context_;
    ///@brief RTP 传输层
    std::shared_ptr<rtp::AsioRtpTransport> rtp_transport_;
    ///@brief 媒体源列表
    std::vector<std::shared_ptr<rtp::MediaSource>> media_sources_;
};

}
