#include "net/tcpsession.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "log/logmanager.h"

TCPSession::TCPSession(net::io_context& ioc) : socket_(ioc), read_buffer_(1024)
{
    boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();
	session_id_ = boost::uuids::to_string(a_uuid);
}

TCPSession::TCPSession(tcp::socket socket) : socket_(std::move(socket)), read_buffer_(1024)
{
    boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();
	session_id_ = boost::uuids::to_string(a_uuid);
}

TCPSession::~TCPSession()
{
    // Stop();
}

void TCPSession::Start()
{

}


void TCPSession::SetBufferSize(std::size_t size)
{
    read_buffer_.resize(size);
}

std::string TCPSession::GetSessionID() const
{
    return session_id_;
}

std::string TCPSession::GetRemoteAddress() const
{
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec)
    {
        return "";
    }
    return socket_.remote_endpoint().address().to_string();
}

int16_t TCPSession::GetRemotePort() const
{
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec)
    {
        return -1;
    }
    return socket_.remote_endpoint().port();
}