#pragma once
#include "net/session.h"
class ProtocolSession {
public:
    explicit ProtocolSession(std::shared_ptr<Session> session)
        : session_(std::move(session)) {
    }

    virtual ~ProtocolSession() = default;
    virtual void OnBytes(const uint8_t* data, size_t size) = 0;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    
protected:
    void Send(const uint8_t* data, size_t size) {
        session_->Send(data, size);
    }
    std::shared_ptr<Session> session_;    
};