#include "rtp/h264source.h"
#include <chrono>

H264Source::H264Source(uint32_t frame_rate) : frame_rate_(frame_rate) {
    media_type_ = H264;
    payload_ = 96;
    clock_rate_ = 90000;
}

H264Source::~H264Source() {
}

// H264Source::Ptr H264Source::Create(uint32_t frame_rate) {
//     return std::make_shared<H264Source>(frame_rate);
// }

void H264Source::SetFrameRate(uint32_t frame_rate) {
    frame_rate_ = frame_rate;
}

uint32_t H264Source::GetFrameRate() const {
    return frame_rate_;
}

std::string H264Source::GetMediaDescription(uint16_t port) {
    return "m=video " + std::to_string(port) + " RTP/AVP " + std::to_string(payload_);
}

std::string H264Source::GetAttribute() {
    return "a=rtpmap:" + std::to_string(payload_) + " H264/" + std::to_string(clock_rate_) + "\r\n";
}

bool H264Source::HandleFrame(MediaChannelId channelId, AVFrame frame) { 
    uint8_t* frame_buf = frame.buffer.get();
    uint32_t frame_size = frame.size;

    if (frame.timestamp == 0) {
        frame.timestamp = GetTimestamp();
    }

    if (frame_size <= MAX_RTP_PAYLOAD_SIZE) {
        // 直接封装发送
        RtpPacket rtp_packet;        
        rtp_packet.size = frame_size + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;
        rtp_packet.timestamp = frame.timestamp;
        rtp_packet.type = frame.type;
        rtp_packet.last = 1;
        memcpy(rtp_packet.data.get() + RTP_HEADER_SIZE + RTP_TCP_HEAD_SIZE, frame_buf, frame_size);
        
        if (send_frame_callback_) {
            if (!send_frame_callback_(channelId, rtp_packet)) {
                return false;
            }
        }
    }
    else{
        // 分片发送
        char FU_A[2] = { 0 };
        FU_A[0] = frame_buf[0] & 0xE0;  // 保留NRI
        FU_A[0] |= 28;  // 28表示FU-A分片 （低5位）
        FU_A[1] = frame_buf[0] & 0x1F;  // 保留类型(低5位)
        FU_A[1] |= 0x80;    // 置位分片开始

        // 跳过 NAL 头
        frame_buf++;
        frame_size--;

        while (frame_size + 2 > MAX_RTP_PAYLOAD_SIZE) {
            // 封装 FU-A 分片
            RtpPacket rtp_packet;        
            rtp_packet.size = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE;
            rtp_packet.timestamp = frame.timestamp;
            rtp_packet.type = frame.type;
            rtp_packet.last = 0;    // 不是最后一个分片
            
            // 填充 FU-A 头
            memcpy(rtp_packet.data.get() + RTP_HEADER_SIZE + RTP_TCP_HEAD_SIZE, FU_A, 2);
            // 填充分片数据
            memcpy(rtp_packet.data.get() + RTP_HEADER_SIZE + RTP_TCP_HEAD_SIZE + 2, frame_buf, MAX_RTP_PAYLOAD_SIZE - 2);

            // 发送分片
            if (send_frame_callback_) {
                if (!send_frame_callback_(channelId, rtp_packet)) {
                    return false;
                }
            }

            // 更新分片数据指针和大小
            frame_buf += (MAX_RTP_PAYLOAD_SIZE - 2);
            frame_size -= (MAX_RTP_PAYLOAD_SIZE - 2);

            // 更新分片开始标志
            FU_A[1] &= ~0x80;
        }

        // 最后一个分片
        {
            RtpPacket rtp_pkt;
            rtp_pkt.type = frame.type;
            rtp_pkt.timestamp = frame.timestamp;
            rtp_pkt.size = 4 + RTP_HEADER_SIZE + 2 + frame_size;
            rtp_pkt.last = 1;   // 最后一个分片

            FU_A[1] |= 0x40;    // 置位分片结束
            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = FU_A[0];
            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = FU_A[1];
            memcpy(rtp_pkt.data.get() + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 2, 
                frame_buf, frame_size);

            if (send_frame_callback_) {
			    if (!send_frame_callback_(channelId, rtp_pkt)) {
				    return false;
			    }              
            }
        }
    }

    return true;
}

uint32_t H264Source::GetTimestamp() {
    auto ts = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());
    return static_cast<uint32_t>((ts.time_since_epoch().count() + 500) / 1000 * 90);    //(clock_rate_ / 1000)
}