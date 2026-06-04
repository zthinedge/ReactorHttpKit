#include "net/TcpServer.h"

#include <iostream>

namespace reactor_http_kit::net
{

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddress, std::string name)
    : loop_(loop)
    , name_(std::move(name))
    , acceptor_(loop, listenAddress)
{
    acceptor_.setNewConnectionCallback(
        [this](int fd, const InetAddress& peerAddress) { handleNewConnection(fd, peerAddress); });
}

TcpServer::~TcpServer() = default;

void TcpServer::start()
{
    acceptor_.listen();
}

void TcpServer::setMessageCallback(TcpConnection::MessageCallback cb)
{
    messageCallback_ = std::move(cb);
}

void TcpServer::handleNewConnection(int fd, const InetAddress& peerAddress)
{
    auto connection = std::make_shared<TcpConnection>(loop_, fd, peerAddress);
    connection->setMessageCallback(messageCallback_);
    connection->setCloseCallback([this](const TcpConnectionPtr& conn) { removeConnection(conn); });
    connections_[fd] = connection;
    std::cout << name_ << " accepted " << peerAddress.toIpPort() << ", fd=" << fd << std::endl;
    connection->connectEstablished();
}

void TcpServer::removeConnection(const TcpConnectionPtr& connection)
{
    if (!connection)
    {
        return;
    }
    std::cout << name_ << " closed " << connection->peerAddress().toIpPort()
              << ", fd=" << connection->fd() << std::endl;
    connections_.erase(connection->fd());
}

} // namespace reactor_http_kit::net
