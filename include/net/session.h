#pragma once
#include <memory>
#include <string>
#include <atomic>

namespace Net {
class Session {
public:
    using DataHandler = std::function<void(const uint8_t*, size_t)>;
    using CloseHandler = std::function<void()>;

    virtual ~Session() = default;

    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void Send(const uint8_t* data, size_t size) = 0;
    virtual void Send(const std::string& data) = 0;

    virtual bool IsRunning() const = 0;
    virtual std::string GetRemoteAddress() const = 0;
    virtual int16_t GetRemotePort() const = 0;
    virtual std::string GetLocalAddress() const = 0;
    virtual int16_t GetLocalPort() const = 0;

    virtual void SetDataHandler(DataHandler handler) = 0;
    virtual void SetCloseHandler(CloseHandler handler) = 0;
protected:
    virtual void OnBytes(const uint8_t* data, size_t size) = 0;
    virtual void OnClose() = 0;
    // virtual void OnError(const std::string& error) = 0;
    std::atomic<bool> running_{ false };

};
}