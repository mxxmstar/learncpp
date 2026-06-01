#pragma once

#include <functional>
#include <utility>

namespace common::runtime {

/// @brief 下游发射回调类型
/// @tparam Frame 数据帧类型
template <typename Frame>
using EmitCallback = std::function<void(Frame)>;

/// @brief 被动处理节点接口
/// @tparam Frame 数据帧类型
///
/// 节点只实现 Process(frame) 处理数据，不创建或管理线程。
/// 处理完成后调用 Emit() 将结果发给下游。
template <typename Frame>
class INode {
public:
    virtual ~INode() = default;

    /// @brief 处理一帧数据
    /// @param frame 输入数据帧
    virtual void Process(Frame frame) = 0;

    /// @brief 注册下游发射回调（由 Runtime 在 AddNode 时自动调用）
    virtual void SetEmitCallback(EmitCallback<Frame> emit) {
        emit_ = std::move(emit);
    }

protected:
    /// @brief 向下游节点发送数据
    /// @param frame 要发射的数据帧
    void Emit(Frame frame) const {
        if (emit_) {
            emit_(std::move(frame));
        }
    }

private:
    /// 下游发射回调（由 Runtime 注入）
    EmitCallback<Frame> emit_;
};

/// @brief 主动源节点接口
/// @tparam Frame 数据帧类型
///
/// 源节点自主产生数据（如摄像头、文件读取），
/// Runtime 在 Start/Stop 中管理其生命周期。
template <typename Frame>
class ISourceNode {
public:
    virtual ~ISourceNode() = default;

    /// @brief 注册下游发射回调（由 Runtime 在 AddSource 时自动调用）
    virtual void SetEmitCallback(EmitCallback<Frame> emit) {
        emit_ = std::move(emit);
    }

    /// @brief 启动数据生产
    virtual void Start() = 0;

    /// @brief 停止数据生产
    virtual void Stop() = 0;

protected:
    /// @brief 向下游节点发送数据
    /// @param frame 要发射的数据帧
    void Emit(Frame frame) const {
        if (emit_) {
            emit_(std::move(frame));
        }
    }

private:
    /// 下游发射回调（由 Runtime 注入）
    EmitCallback<Frame> emit_;
};

} // namespace common::runtime
