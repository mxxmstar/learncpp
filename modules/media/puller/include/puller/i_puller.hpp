#pragma once

#include <functional>
#include <memory>
#include <string>

#include "defines/media_packet.hpp"
#include "stream/stream_info.h"

/// @brief 拉流器接口（协议适配层）
///
/// 仅负责三件事：
///   1. Open()       — 建立底层传输连接
///   2. ReadPacket() — 从网络读取一个媒体包
///   3. Close()      — 断开底层连接
///
/// 不负责：
///   - 重连 / watchdog / 状态机
///   - 统计 / 超时检测
///   - decoder / pipeline / source 管理
///
/// 上述功能统一由 StreamSession 和 StreamSource 上层管理。
class IPuller {
public:
    virtual ~IPuller() = default;

    // ==================== 生命周期 ====================

    /// @brief 打开流
    /// @param url 流地址（如 rtsp://…、http://…）
    /// @return true 连接成功
    virtual bool Open(const std::string& url) = 0;

    /// @brief 关闭流
    virtual void Close() = 0;

    /// @brief 读取一个媒体包
    /// @param[out] packet 输出包（成功时为有效对象，失败时为 nullptr）
    /// @return true 成功读取；false 读取失败或流结束
    ///
    /// 当返回 false 时，调用方可通过 GetStreamInfo() 判断区别：
    /// 若 info 有效则为 EOF，否则为错误。
    virtual bool ReadPacket(std::shared_ptr<MediaPacket>& packet) = 0;

    // ==================== 元数据 ====================

    /// @brief 获取流信息
    virtual StreamInfo GetStreamInfo() const = 0;

    // ==================== 回调 ====================

    /// @brief 拉流器层事件回调（协议异常等）
    using EventCallback = std::function<void(const std::string&)>;

    /// @brief 设置事件回调
    virtual void SetEventCallback(EventCallback cb) = 0;
};
