#include "net/Acceptor.h"

#include "net/EventLoop.h"

#include <cerrno>
#include <iostream>
#include <unistd.h>

namespace reactor_http_kit::net
{

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddress)
    : loop_(loop)
    , acceptSocket_(std::make_unique<Socket>(createNonblockingSocket()))
    , acceptChannel_(std::make_unique<Channel>(loop, acceptSocket_->fd()))
{
    acceptSocket_->setreuseaddr(true);
    acceptSocket_->setreuseport(true);
    acceptSocket_->bind(listenAddress);
    acceptChannel_->setReadCallback([this]() { handleRead(); });
}

Acceptor::~Acceptor()
{
    acceptChannel_->remove();
}

void Acceptor::listen()
{
    listening_ = true;
    acceptSocket_->listen();
    acceptChannel_->enableReading();
}

void Acceptor::setNewConnectionCallback(NewConnectionCallback cb)
{
    newConnectionCallback_ = std::move(cb);
}

void Acceptor::handleRead()
{
    while (true)
    {
        InetAddress peerAddress;
        int connectionFd = acceptSocket_->accept(peerAddress);
        if (connectionFd >= 0)
        {
            if (newConnectionCallback_)
            {
                newConnectionCallback_(connectionFd, peerAddress);
            }
            else
            {
                ::close(connectionFd);
            }
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            break;
        }
        if (errno != EINTR)
        {
            std::cerr << "accept() failed, errno=" << errno << std::endl;
            break;
        }
    }
}

} // namespace reactor_http_kit::net
