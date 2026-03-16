#include <iostream>
#include <cassert>
#include <cstring>
#include "rtp/rtp.h"
#include "rtp/media.h"
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif // _WIN32
void test_rtp_header_size() {
    std::cout << "=== 测试 RTP 头部大小 ===" << std::endl;
    assert(RTP_HEADER_SIZE == 12);
    std::cout << "RTP 头部大小：" << RTP_HEADER_SIZE << " 字节 ✓" << std::endl;
    std::cout << std::endl;
}

void test_rtp_packet_creation() {
    std::cout << "=== 测试 RTP 包创建 ===" << std::endl;
    
    RtpPacket packet;
    
    // 测试默认值
    assert(packet.data != nullptr);
    assert(packet.size == 0);
    assert(packet.timestamp == 0);
    assert(packet.type == 0);
    assert(packet.last == 0);
    
    std::cout << "RTP 包默认值正确 ✓" << std::endl;
    
    // 设置包数据
    packet.size = 100;
    packet.timestamp = 90000;
    packet.type = VIDEO_FRAME_I;
    packet.last = 1;
    
    std::cout << "包大小：" << packet.size << std::endl;
    std::cout << "时间戳：" << packet.timestamp << std::endl;
    std::cout << "帧类型：" << (int)packet.type << std::endl;
    std::cout << "是否最后一帧：" << (int)packet.last << std::endl;
    std::cout << std::endl;
}

void test_rtp_header_structure() {
    std::cout << "=== 测试 RTP 头部结构 ===" << std::endl;
    
    RtpHeader header;
    memset(&header, 0, sizeof(header));
    
    // 设置字段值
    header.version = RTP_VERSION;
    header.padding = 0;
    header.extension = 0;
    header.csrc = 0;
    header.marker = 1;
    header.payload = H264;
    header.seq = htons(1234);
    header.ts = htonl(5678);
    header.ssrc = htonl(0x12345678);
    
    std::cout << "RTP 版本：" << (int)header.version << std::endl;
    std::cout << "标记位：" << (int)header.marker << std::endl;
    std::cout << "负载类型：" << (int)header.payload << std::endl;
    std::cout << "序列号：" << ntohs(header.seq) << std::endl;
    std::cout << "时间戳：" << ntohl(header.ts) << std::endl;
    std::cout << "SSRC: 0x" << std::hex << ntohl(header.ssrc) << std::dec << std::endl;
    std::cout << std::endl;
}

void test_media_channel_info() {
    std::cout << "=== 测试媒体通道信息 ===" << std::endl;
    
    MediaChannelInfo info;
    memset(&info, 0, sizeof(info));
    
    info.local_rtp_channel = 0;
    info.local_rtcp_channel = 1;
    info.local_rtp_port = 5004;
    info.local_rtcp_port = 5005;
    info.packet_seq = 0;
    info.clock_rate = 90000;
    info.is_setup = true;
    info.is_playing = true;
    
    std::cout << "RTP 端口：" << info.local_rtp_port << std::endl;
    std::cout << "RTCP 端口：" << info.local_rtcp_port << std::endl;
    std::cout << "时钟率：" << info.clock_rate << std::endl;
    std::cout << "已设置：" << info.is_setup << std::endl;
    std::cout << "播放中：" << info.is_playing << std::endl;
    std::cout << std::endl;
}

void test_frame_types() {
    std::cout << "=== 测试帧类型 ===" << std::endl;
    
    std::cout << "视频 I 帧：" << (int)VIDEO_FRAME_I << std::endl;
    std::cout << "视频 P 帧：" << (int)VIDEO_FRAME_P << std::endl;
    std::cout << "视频 B 帧：" << (int)VIDEO_FRAME_B << std::endl;
    std::cout << "音频帧：" << (int)AUDIO_FRAME << std::endl;
    std::cout << std::endl;
}

void test_media_types() {
    std::cout << "=== 测试媒体类型 ===" << std::endl;
    
    std::cout << "PCMA: " << PCMA << std::endl;
    std::cout << "H264: " << H264 << std::endl;
    std::cout << "AAC: " << AAC << std::endl;
    std::cout << "H265: " << H265 << std::endl;
    std::cout << std::endl;
}

void test_avframe() {
    std::cout << "=== 测试 AVFrame ===" << std::endl;
    
    uint32_t frame_size = 1024;
    AVFrame frame(frame_size);
    
    assert(frame.buffer != nullptr);
    assert(frame.size == frame_size);
    
    // 填充一些测试数据
    memset(frame.buffer.get(), 0xAB, frame_size);
    
    frame.type = VIDEO_FRAME_I;
    frame.timestamp = 1000;
    
    std::cout << "帧大小：" << frame.size << std::endl;
    std::cout << "帧类型：" << (int)frame.type << std::endl;
    std::cout << "时间戳：" << frame.timestamp << std::endl;
    std::cout << "缓冲区首字节：0x" << std::hex << (int)frame.buffer.get()[0] << std::dec << std::endl;
    std::cout << std::endl;
}

void test_transport_mode() {
    std::cout << "=== 测试传输模式 ===" << std::endl;
    
    std::cout << "RTP over TCP: " << RTP_OVER_TCP << std::endl;
    std::cout << "RTP over UDP: " << RTP_OVER_UDP << std::endl;
    std::cout << "RTP over Multicast: " << RTP_OVER_MULTICAST << std::endl;
    std::cout << std::endl;
}

int main() {
#ifdef _WIN32
    system("chcp 65001");
#endif // _WIN32

    
    std::cout << "========================================" << std::endl;
    std::cout << "       RTP 模块基础测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    try {
        test_rtp_header_size();
        test_rtp_packet_creation();
        test_rtp_header_structure();
        test_media_channel_info();
        test_frame_types();
        test_media_types();
        test_avframe();
        test_transport_mode();
        
        std::cout << "========================================" << std::endl;
        std::cout << "       所有测试通过！✓" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试失败：" << e.what() << std::endl;
        return 1;
    }
}