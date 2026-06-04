#pragma once

#include "net/Channel.h"
#include "net/InetAddress.h"
#include "net/Socket.h"

#include <functional>
#include <memory>

namespace reactor_http_kit::net
{

class EventLoop;

class Acceptor
{
public:
    using NewConnectionCallback = std::function<void(int, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddress);
    ~Acceptor();

    Acceptor(const Acceptor&) = delete;
    Acceptor& operator=(const Acceptor&) = delete;

    void listen();
    void setNewConnectionCallback(NewConnectionCallback cb);

private:
    void handleRead();

private:
    EventLoop* loop_ { nullptr };
    std::unique_ptr<Socket> acceptSocket_;
    std::unique_ptr<Channel> acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_ { false };
};

} // namespace reactor_http_kit::net
