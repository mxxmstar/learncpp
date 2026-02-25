#pragma once
#include <string>
#include "net/tcpserver.h"
#include "net/tcpsession.h"
#include "net/httpsession.h"
#include "net/httpserver.h"

class M_RTSPSession : public AsioHttpSession {
public:
    explicit M_RTSPSession(tcp::socket socket)
        : AsioHttpSession(std::move(socket)) {}

protected:
    void OnBytes(const uint8_t* data, size_t size) override {
        // 回显数据
        /*std::cout << "Received: " << std::string(reinterpret_cast<const char*>(data), size) << std::endl;
        Send(data, size);*/
    }

    void OnClose() override {
       /* LOG_MAIN_INFO_AT("EchoSession closed: {}", GetSessionID());*/
    }

};