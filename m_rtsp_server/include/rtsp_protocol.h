#pragma once
#include <cstdint>
#include <string>
#include <map>
namespace mx {
enum class RtspState {
	INIT,	// 初始状态，等待客户端发送请求
	READY,	// 已经收到SETUP请求，准备好播放
	PLAYING,// 正在播放
	PAUSED,	// 已经暂停
	TEARDOWN// 已经结束
};

enum class RtspMethod {
	OPTIONS,
	DESCRIBE,
	SETUP,
	PLAY,
	PAUSE,
	TEARDOWN,
	UNKNOWN
};

namespace RtspStatus {
	const int OK = 200;
	const int BadRequest = 400;
	const int NotFound = 404;
	const int MethodNotAllowed = 405;
	const int SessionNotFound = 454;
	const int InternalServerError = 500;
}

struct RtspRequest { 
	RtspMethod method{ RtspMethod::UNKNOWN };	// 请求方法
	std::string url;	// 请求URL
	std::string version;	// RTSP版本号
	std::map<std::string, std::string> headers;	// 请求头字段
	std::string body;	// 请求体
};

struct RtspResponse { 
	int status_code{ 200 };	// 状态码
	std::string status_reason{ "OK" };	// 状态原因
	std::map<std::string, std::string> headers;	// 响应头字段
	std::string body;	// 响应体
	std::string content_type{ "application/sdp" };	// 响应体的MIME类型
};

/// @brief RTSP会话上下文，保存当前会话的状态、请求信息等数据
struct RtspSessionContext {
	std::string session_id;	// RTSP会话ID
	RtspState state{RtspState::INIT};	// 当前状态
	std::string cseq{ };	// 当前请求的CSeq
	std::string url;	// 当前请求的URL	
	uint16_t client_rtp_port{ 0 };	// 客户端RTP端口号(TCP时不需要)
	uint16_t client_rtcp_port{ 0 };	// 客户端RTCP端口号
	uint32_t rtp_timestamp{ 0 };	// RTP时间戳
	uint16_t rtp_seq_num{ 0 };	// RTP序列号
	bool is_tcp{ true };	// 是否使用TCP传输
	int rtp_channel{ 0 };	// RTP通道号(TCP时使用)
	int rtcp_channel{ 1 };	// RTCP通道号(TCP时使用)
};	

class RtspProtocol {
public:

	/// @brief 构建RTSP响应字符串
	/// @param response 响应结构体
	/// @param cseq 请求的CSeq
	/// @return 响应字符串
	static std::string BuildResponse(const RtspResponse& response, std::string cseq);

	/// @brief 解析RTSP请求字符串
	/// @param request_str 请求字符串
	/// @param request 输出请求结构体
	/// @return 是否成功解析
	static bool ParseRequest(const std::string& request_str, RtspRequest& request);

	/// @brief 解析RTSP请求行
	/// @param request_line 请求行字符串
	/// @param method 输出请求方法
	/// @param url 输出请求URL
	/// @param version 输出RTSP版本号
	/// @return 是否成功解析
	static bool ParseRtspRequestLine(const std::string& request_line, RtspMethod& method, std::string& url, std::string& version);

	/// @brief 解析RTSP请求头
	/// @param request 请求字符串
	/// @return 头部字段映射
	static std::map<std::string, std::string> ParseHeaders(const std::string& request);

	/// @brief 生成SDP描述
	static std::string GenerateSdp(const std::string ip, std::string& session_id);
};


}