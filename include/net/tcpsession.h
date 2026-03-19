#pragma once
#include <boost/asio.hpp>
#include <stdint.h>
#include <queue>

#include "session.h"
#include "databuffer.h"


namespace net = boost::asio;

class AsioTCPSession : public Session, public std::enable_shared_from_this<AsioTCPSession>
{
public:
    using tcp = boost::asio::ip::tcp;

    explicit AsioTCPSession(net::io_context& ioc);
    explicit AsioTCPSession(tcp::socket socket);
    ~AsioTCPSession();

    void Start() override;
    void Stop() override;
    void Send(const std::string& data) override;
    void Send(const uint8_t* data, size_t size) override;


    bool IsRunning() const ;
    std::string GetSessionID() const;
    std::string GetRemoteAddress() const override;
    int16_t GetRemotePort() const override;
    std::string GetLocalAddress() const override;
    int16_t GetLocalPort() const override;

    void SetDataHandler(DataHandler handler) override;
    void SetCloseHandler(CloseHandler handler) override;

    int NativeFd();
    /// @brief 释放 asio 控制权
    tcp::socket DetachSocket();
    /// @brief 停止 AsyncRead
    void StopRead();
    /// @brief 获取 io_context
    boost::asio::io_context& GetIOContext();

protected:
    void AsyncRead();
    void AsyncWrite();

    tcp::socket socket_;

private:
    // 子类只关心"收到字节"
    void OnBytes(const uint8_t* data, std::size_t size) override;
    void OnClose() override;
    void OnError(boost::system::error_code ec);
            
    
    std::vector<uint8_t> read_buffer_;
    std::string session_id_;
    std::atomic<bool> closed_{ false };

    std::queue<std::shared_ptr<std::vector<uint8_t>>> send_queue_;
    bool writing_ { false };
    
    DataHandler data_handler_;
    CloseHandler close_handler_;
};

