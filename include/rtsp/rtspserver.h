#pragma once
#include <cstdint>
#include "net/tcpserver.h"
#include "net/asio_io_context_pool.h"
#include "rtsp/rtspsession.h"

namespace rtsp {

class RtspServer {
public:
    RtspServer(boost::asio::io_context& io_context, 
               AsioIOContextPool& work_pool, 
               uint16_t port);
    void Start();
    void OnCreateSession(boost::asio::ip::tcp::socket socket);

private:
    AsioTCPServer server_;
};

}
