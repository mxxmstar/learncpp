// EncodeNode: 同步编码节点
// 继承 INode<FramePtr> 框架，以同步方式完成帧到编码包的转换
// 接收 FramePtr 输入，编码后通过 PacketCallback 回调输出 PacketPtr

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "common/runtime/node.h"
#include "defines/media_frame.hpp"
#include "defines/media_packet.hpp"
#include "encoder/i_encoder.hpp"

class EncodeNode : public common::runtime::INode<FramePtr> {
public:
    using PacketCallback = std::function<void(PacketPtr)>;

    EncodeNode();
    explicit EncodeNode(std::unique_ptr<IEncoder> encoder);
    ~EncodeNode() override;

    EncodeNode(const EncodeNode&) = delete;
    EncodeNode& operator=(const EncodeNode&) = delete;

    // 打开编码器并初始化配置
    bool Init(const EncoderConfig& config);
    // INode 接口: 处理一帧输入
    void Process(FramePtr frame) override;

    // 直接调用者的便捷包装（未使用 INode 框架时使用）
    bool PushFrame(MediaFrame frame);

    // 设置编码完成回调函数
    void SetPacketCallback(PacketCallback cb);
    // 关闭节点，刷新编码器并释放资源
    void Close();

private:
    // 编码一帧的核心逻辑，由 Process 和 PushFrame 内部调用
    bool EncodeFrame(FramePtr frame);
    // 将编码后的包逐个分发给回调
    void DispatchPackets(const std::vector<PacketPtr>& packets);

    std::unique_ptr<IEncoder> encoder_;     // 底层编码器
    std::mutex encode_mutex_;               // 编码操作互斥锁（序列化 EncodeFrame/Close）
    std::mutex callback_mutex_;             // 回调函数互斥锁
    PacketCallback packet_cb_;              // 编码结果回调
    bool opened_{false};                    // 编码器是否已打开
};
