#pragma once
#include <memory>
#include <cstdint>

/// @brief RTP包头部大小
#define RTP_HEADER_SIZE 12
#define MAX_RTP_PAYLOAD_SIZE   1420 //1460  1500-20-12-8
#define RTP_VERSION			   2
/// @brief RTP包TCP头部大小
#define RTP_TCP_HEAD_SIZE	   4
#define RTP_VPX_HEAD_SIZE	   1

enum TransportMode
{
    RTP_OVER_TCP = 1,   // 复用 RTSP socket
    RTP_OVER_UDP = 2,   // 独立的 UDP socket
    RTP_OVER_MULTICAST = 3, // 组播 UDP
};

/// @brief RTP包头部是否为大端序
#define RTP_HEADER_BIG_ENDIAN 0
struct RtpHeader {
#if RTP_HEADER_BIG_ENDIAN
    /* 大端序 */
    unsigned char version   : 2;
    unsigned char padding   : 1;
    unsigned char extension : 1;
    unsigned char csrc      : 4;
    unsigned char marker    : 1;
    unsigned char payload   : 7;
#else
    /* 小端序 */
    unsigned char csrc      : 4;
    unsigned char extension : 1;
    unsigned char padding   : 1;
    unsigned char version   : 2;
    unsigned char payload   : 7;
    unsigned char marker    : 1;
#endif 
    unsigned short seq;
    unsigned int   ts;
    unsigned int   ssrc;
};

/// @brief 媒体通道信息
struct MediaChannelInfo
{
    RtpHeader rtp_header;	// 该通道RTP包头部

    // tcp
    uint16_t local_rtp_channel;	// 该通道RTP通道号
    uint16_t local_rtcp_channel;	// 该通道RTCP通道号

    // udp
    uint16_t local_rtp_port;	// 该通道RTP端口号
    uint16_t local_rtcp_port;	// 该通道RTCP端口号
    uint16_t packet_seq;	// 该通道RTP包序号
    uint32_t clock_rate;	// 该通道RTP包时钟率

    // rtcp
    uint64_t packet_count;	// 该通道已发送RTP包数量
    uint64_t octet_count;	// 该通道已发送RTP包字节数, 不包含RTP包头部
    uint64_t last_rtcp_ntp_time;	// 该通道上次发送RTCP包时间, 单位为秒

    bool is_setup;	// 该通道是否已设置
    bool is_playing;	// 该通道是否处于播放状态
    bool is_recording;	// 该通道是否处于录制状态
};

struct RtpPacket
{
    RtpPacket(): data(new uint8_t[1600], std::default_delete<uint8_t[]>())
    {
        type = 0;
        size = 0;
        timestamp = 0;
        last = 0;
    }

    std::shared_ptr<uint8_t> data;  // 包数据
    std::size_t size;   // 包大小
    std::size_t timestamp;  // 时间戳
    uint8_t type;   // 帧类型
    uint8_t last;   // 是否为最后一帧
};
