#pragma once
#include <string>
#include "net/tcpserver.h"
#include "net/tcpsession.h"
#include "rtsp_protocol.h"
namespace mx {

class RtspSession : public AsioTCPSession {
public:
    explicit RtspSession(tcp::socket socket);

protected:
    void OnBytes(const uint8_t* data, size_t size) override;

    void OnClose() override;    

private:
	//RtspSessionContext context_;
	/// @brief 处理RTSP请求
	void handleRtspRequest(const std::string& request);
    
    /// @brief 构建RTSP响应
    std::string buildResponse(const RtspResponse& response, std::string cseq);

    /// @brief 处理OPTIONS请求
    std::string handleOptions(const std::map<std::string, std::string>& headers);
    /// @brief 处理DESCRIBE请求
    std::string handleDescribe(const std::map<std::string, std::string>& headers);
    /// @brief 处理SETUP请求
    std::string handleSetup(const std::map<std::string, std::string>& headers);
    /// @brief 处理PLAY请求
    std::string handlePlay(const std::map<std::string, std::string>& headers);
    /// @brief 处理TEARDOWN请求
    std::string handleTeardown(const std::map<std::string, std::string>& headers);
    /// @brief 处理PAUSE请求
    std::string handlePause(const std::map<std::string, std::string>& headers);

    std::string version_;
    RtspSessionContext context_;
};

}