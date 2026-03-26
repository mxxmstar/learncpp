#include "net/websocket_router.h"
#include "log/logmanager.h"
#include <boost/json.hpp>

namespace Json = boost::json;

namespace Net {

WebSocketRouter& WebSocketRouter::GetInstance() {
    static WebSocketRouter instance;
    return instance;
}

void WebSocketRouter::RegisterMessageHandler(const std::string& msg_type, MessageHandler handler) {
    std::lock_guard lock(mutex_);
    message_handlers_[msg_type] = std::move(handler);
}

void WebSocketRouter::SetConnectHandler(ConnectHandler handler) {
    connect_handler_ = std::move(handler);
}

void WebSocketRouter::SetDisconnectHandler(DisconnectHandler handler) {
    disconnect_handler_ = std::move(handler);
}

void WebSocketRouter::DispatchMessage(const std::string& session_id, const std::string& message) {
    std::string msg_type;
    
    try {
        auto jv = Json::parse(message);
        if (jv.is_object()) {
            const auto& obj = jv.as_object();
            if (obj.contains("type")) {
                msg_type = obj.at("type").as_string().c_str();
            }
        }
    } catch (std::exception& e) {
        LOG_MAIN_ERROR_AT("WebSocketRouter parse message error: {}", e.what());
        return;
    }

    std::lock_guard lock(mutex_);
    auto it = message_handlers_.find(msg_type);
    if (it != message_handlers_.end()) {
        it->second(session_id, message);
    } else {
        LOG_MAIN_WARN_AT("WebSocketRouter no handler for msg_type: {}", msg_type);
    }
}

void WebSocketRouter::OnConnect(const std::string& session_id) {
    LOG_MAIN_INFO_AT("WebSocket client connected: {}", session_id);
    if (connect_handler_) {
        connect_handler_(session_id);
    }
}

void WebSocketRouter::OnDisconnect(const std::string& session_id) {
    LOG_MAIN_INFO_AT("WebSocket client disconnected: {}", session_id);
    {
        std::lock_guard lock(mutex_);
        sessions_.erase(session_id);
    }
    if (disconnect_handler_) {
        disconnect_handler_(session_id);
    }
}

void WebSocketRouter::SendTo(const std::string& session_id, const std::string& message) {
    std::lock_guard lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        auto session = it->second.lock();
        if (session) {
            session->Send(message);
        }
    }
}

void WebSocketRouter::BindSession(const std::string& session_id, std::shared_ptr<AsioWebSocketSession> session) {
    std::lock_guard lock(mutex_);
    sessions_[session_id] = session;
    OnConnect(session_id);
}

void WebSocketRouter::UnbindSession(const std::string& session_id) {
    OnDisconnect(session_id);
}

}
