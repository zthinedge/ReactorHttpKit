#pragma once

#include "net/Acceptor.h"
#include "net/TcpConnection.h"

#include <map>
#include <memory>
#include <string>

namespace reactor_http_kit::net
{

class EventLoop;

class TcpServer
{
public:
    TcpServer(EventLoop* loop, const InetAddress& listenAddress, std::string name);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void start();
    void setMessageCallback(TcpConnection::MessageCallback cb);

private:
    void handleNewConnection(int fd, const InetAddress& peerAddress);
    void removeConnection(const TcpConnectionPtr& connection);

private:
    EventLoop* loop_ { nullptr };
    std::string name_;
    Acceptor acceptor_;
    TcpConnection::MessageCallback messageCallback_;
    std::map<int, TcpConnectionPtr> connections_;
};

} // namespace reactor_http_kit::net
