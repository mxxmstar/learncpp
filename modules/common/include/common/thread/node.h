#pragma once

#include <functional>
#include <utility>

namespace common::thread {

template <typename Frame>
using EmitCallback = std::function<void(Frame)>;

template <typename Frame>
class INode {
public:
    virtual ~INode() = default;

    virtual void Process(Frame frame) = 0;

    virtual void SetEmitCallback(EmitCallback<Frame> emit) {
        emit_ = std::move(emit);
    }

protected:
    void Emit(Frame frame) const {
        if (emit_) {
            emit_(std::move(frame));
        }
    }

private:
    EmitCallback<Frame> emit_;
};

template <typename Frame>
class ISourceNode {
public:
    virtual ~ISourceNode() = default;

    virtual void SetEmitCallback(EmitCallback<Frame> emit) {
        emit_ = std::move(emit);
    }

    virtual void Start() = 0;
    virtual void Stop() = 0;

protected:
    void Emit(Frame frame) const {
        if (emit_) {
            emit_(std::move(frame));
        }
    }

private:
    EmitCallback<Frame> emit_;
};

} // namespace common::thread
