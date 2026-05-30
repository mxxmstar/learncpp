#pragma once
/// @file i_decoder.hpp
/// 解码器纯虚接口，所有具体解码器实现均需继承此类。

#include <functional>
#include <memory>

#include "defines/media_packet.hpp"
#include "defines/media_frame.hpp"
#include "stream/stream_info.h"

/// @brief 解码器抽象接口
///
/// 职责：
///   - Open()       — 根据 StreamInfo 初始化解码器
///   - Decode()     — 输入一个 MediaPacket，输出零到多个 MediaFrame（通过回调）
///   - Close()      — 销毁解码器资源
///
/// 不负责：
///   - 包缓存 / 排序（由上游保证送入顺序正确的包）
///   - 帧后处理（缩放/色彩空间转换等应由下游 Pipeline 完成）
class IDecoder {
public:
    /// @brief 解码帧回调
    using FrameCallback = std::function<void(std::shared_ptr<MediaFrame>)>;

    virtual ~IDecoder() = default;

    // ==================== 生命周期 ====================

    /// @brief 打开解码器
    /// @param info 流元信息（编码格式、分辨率、extra_data 等）
    /// @return true 初始化成功
    virtual bool Open(const StreamInfo& info) = 0;

    /// @brief 关闭解码器，释放所有资源
    virtual void Close() = 0;

    // ==================== 解码 ====================

    /// @brief 解码一个媒体包
    /// @param packet 待解码的压缩包（按 pts 顺序送入）
    /// @return true 解码调用成功（注意：可能尚未产生帧）
    ///
    /// 解码产生的帧通过 SetFrameCallback 注册的回调逐帧返回。
    /// 返回 false 表示解码器内部错误，应关闭重建。
    virtual bool Decode(std::shared_ptr<MediaPacket> packet) = 0;

    // ==================== 回调 ====================

    /// @brief 设置解码帧回调
    /// @param cb 每次解码出一帧时被调用
    virtual void SetFrameCallback(FrameCallback cb) = 0;
};
