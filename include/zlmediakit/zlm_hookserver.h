#pragma once
#include "log/logmanager.h"
#include "net/httpserver.h"
#include "net/httpsession.h"
#include "net/httprouter.h"
#include <memory>
#include <boost/json.hpp>
#include <boost/asio.hpp>

namespace json = boost::json;
class ZLMHookServer {
public:
    ZLMHookServer(uint16_t port);

    void Start();
    void Stop();
private:
    void DoAccept();
    void RegisterHooks();
    boost::asio::io_context& accept_ioc_;
    AsioIOContextPool& worker_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> running_{false};

};

