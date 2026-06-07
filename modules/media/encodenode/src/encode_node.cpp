// EncodeNode: 同步编码节点实现
// 在 encode_mutex_ 保护下同步编码，编码结果通过回调分发给消费者

#include "encodenode/encode_node.hpp"

#include "common/log/logmanager.h"
#include "encoder/ffmpeg_encoder.hpp"

#include <utility>

// 默认构造，自动创建 FFmpeg 编码器
EncodeNode::EncodeNode()
    : encoder_(std::make_unique<FFmpegEncoder>()) {
}

// 注入外部编码器
EncodeNode::EncodeNode(std::unique_ptr<IEncoder> encoder)
    : encoder_(std::move(encoder)) {
}

EncodeNode::~EncodeNode() {
    Close();
}

// 初始化: 关闭旧状态，打开编码器
bool EncodeNode::Init(const EncoderConfig& config) {
    Close();

    if (!encoder_) {
        encoder_ = std::make_unique<FFmpegEncoder>();
    }

    std::lock_guard<std::mutex> lock(encode_mutex_);
    if (!encoder_->Open(config)) {
        LOG_MAIN_ERROR_AT("EncodeNode: Init failed: encoder Open failed");
        opened_ = false;
        return false;
    }

    opened_ = true;
    return true;
}

// INode 接口: 处理一帧输入
void EncodeNode::Process(FramePtr frame) {
    (void)EncodeFrame(std::move(frame));
}

// 直接调用者的便捷包装: MediaFrame -> shared_ptr -> EncodeFrame
bool EncodeNode::PushFrame(MediaFrame frame) {
    return EncodeFrame(std::make_shared<MediaFrame>(std::move(frame)));
}

// 设置编码完成回调（线程安全）
void EncodeNode::SetPacketCallback(PacketCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    packet_cb_ = std::move(cb);
}

// 关闭节点: 刷新编码器剩余帧，关闭编码器，释放资源
void EncodeNode::Close() {
    std::vector<PacketPtr> packets;
    {
        std::lock_guard<std::mutex> lock(encode_mutex_);
        if (!encoder_ || !opened_) {
            return;
        }

        // 传入 nullptr 触发编码器输出所有缓冲帧
        if (!encoder_->Encode(nullptr, packets)) {
            LOG_MAIN_ERROR_AT("EncodeNode: encoder flush failed");
        }
        encoder_->Close();
        opened_ = false;
    }

    DispatchPackets(packets);
}

// 核心编码逻辑: 在互斥锁保护下同步编码
bool EncodeNode::EncodeFrame(FramePtr frame) {
    if (!frame) {
        LOG_MAIN_ERROR_AT("EncodeNode: Process rejected null frame");
        return false;
    }

    std::vector<PacketPtr> packets;
    {
        std::lock_guard<std::mutex> lock(encode_mutex_);
        if (!encoder_ || !opened_) {
            LOG_MAIN_ERROR_AT("EncodeNode: Process rejected: node is not initialized");
            return false;
        }

        if (!encoder_->Encode(std::move(frame), packets)) {
            LOG_MAIN_ERROR_AT("EncodeNode: encoder Encode failed");
            return false;
        }
    }

    DispatchPackets(packets);
    return true;
}

// 将编码后的包逐个通过回调分发出去
void EncodeNode::DispatchPackets(const std::vector<PacketPtr>& packets) {
    if (packets.empty()) {
        return;
    }

    // 短暂持有回调锁取出回调对象
    PacketCallback cb;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        cb = packet_cb_;
    }

    if (!cb) {
        return;
    }

    for (const auto& packet : packets) {
        if (packet) {
            cb(packet);
        }
    }
}
