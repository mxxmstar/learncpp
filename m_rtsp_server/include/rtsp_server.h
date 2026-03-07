#pragma  once
#include "net/tcpserver.h"
#include "net/asio_io_context_pool.h"
#include "rtsp_session.h"
#include <cstdint>
namespace mx {
class RtspServer {
public:
    RtspServer(boost::asio::io_context& io_context, AsioIOContextPool& work_pool, uint16_t port);
    void Start();
    void OnCreateSession(boost::asio::ip::tcp::socket socket);
private:
    AsioTCPServer server_;
};

}