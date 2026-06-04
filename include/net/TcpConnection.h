#pragma once

#include "net/Buffer.h"
#include "net/Channel.h"
#include "net/InetAddress.h"
#include "net/Socket.h"

#include <functional>
#include <memory>
#include <string>

namespace reactor_http_kit::net
{

class EventLoop;
class TcpConnection;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
    using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer&)>;
    using CloseCallback = std::function<void(const TcpConnectionPtr&)>;

    TcpConnection(EventLoop* loop, int fd, InetAddress peerAddress);
    ~TcpConnection();

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    int fd() const;
    const InetAddress& peerAddress() const;

    void send(const std::string& data);
    void closeAfterWrite();
    void connectEstablished();

    void setMessageCallback(MessageCallback cb);
    void setCloseCallback(CloseCallback cb);

private:
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();

private:
    EventLoop* loop_ { nullptr };
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    InetAddress peerAddress_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;
    MessageCallback messageCallback_;
    CloseCallback closeCallback_;
    bool closeAfterWrite_ { false };
    bool closed_ { false };
};

} // namespace reactor_http_kit::net
