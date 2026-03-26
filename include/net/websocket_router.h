#pragma once

#include <map>
#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "net/websocket_session.h"

namespace Net {

class WebSocketRouter {
public:
    using MessageHandler = std::function<void(const std::string& session_id, const std::string& message)>;
    using ConnectHandler = std::function<void(const std::string& session_id)>;
    using DisconnectHandler = std::function<void(const std::string& session_id)>;

    static WebSocketRouter& GetInstance();

    void RegisterMessageHandler(const std::string& msg_type, MessageHandler handler);

    void SetConnectHandler(ConnectHandler handler);
    void SetDisconnectHandler(DisconnectHandler handler);

    void DispatchMessage(const std::string& session_id, const std::string& message);

    void OnConnect(const std::string& session_id);
    void OnDisconnect(const std::string& session_id);

    void SendTo(const std::string& session_id, const std::string& message);

    void BindSession(const std::string& session_id, std::shared_ptr<AsioWebSocketSession> session);
    void UnbindSession(const std::string& session_id);

private:
    WebSocketRouter() = default;

    std::map<std::string, MessageHandler> message_handlers_;
    ConnectHandler connect_handler_;
    DisconnectHandler disconnect_handler_;

    std::unordered_map<std::string, std::weak_ptr<AsioWebSocketSession>> sessions_;
    std::mutex mutex_;
};

}
