#include "net/TcpConnection.h"

#include "net/EventLoop.h"

#include <cerrno>
#include <iostream>

namespace reactor_http_kit::net
{

TcpConnection::TcpConnection(EventLoop* loop, int fd, InetAddress peerAddress)
    : loop_(loop)
    , socket_(std::make_unique<Socket>(fd))
    , channel_(std::make_unique<Channel>(loop, fd))
    , peerAddress_(std::move(peerAddress))
{
    socket_->settcpnodelay(true);
    socket_->setkeepalive(true);

    channel_->setReadCallback([this]() { handleRead(); });
    channel_->setWriteCallback([this]() { handleWrite(); });
    channel_->setCloseCallback([this]() { handleClose(); });
    channel_->setErrorCallback([this]() { handleError(); });
}

TcpConnection::~TcpConnection()
{
    channel_->remove();
}

int TcpConnection::fd() const
{
    return socket_->fd();
}

const InetAddress& TcpConnection::peerAddress() const
{
    return peerAddress_;
}

void TcpConnection::send(const std::string& data)
{
    if (closed_)
    {
        return;
    }
    outputBuffer_.append(data);
    channel_->enableWriting();
}

void TcpConnection::closeAfterWrite()
{
    closeAfterWrite_ = true;
    if (outputBuffer_.empty())
    {
        handleClose();
    }
}

void TcpConnection::connectEstablished()
{
    channel_->enableReading();
}

void TcpConnection::setMessageCallback(MessageCallback cb)
{
    messageCallback_ = std::move(cb);
}

void TcpConnection::setCloseCallback(CloseCallback cb)
{
    closeCallback_ = std::move(cb);
}

void TcpConnection::handleRead()
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(fd(), &savedErrno);
    if (n > 0)
    {
        if (messageCallback_)
        {
            messageCallback_(shared_from_this(), inputBuffer_);
        }
    }
    else if (n == 0)
    {
        handleClose();
    }
    else if (savedErrno != EAGAIN && savedErrno != EWOULDBLOCK)
    {
        handleError();
        handleClose();
    }
}

void TcpConnection::handleWrite()
{
    int savedErrno = 0;
    ssize_t n = outputBuffer_.writeFd(fd(), &savedErrno);
    if (n < 0)
    {
        if (savedErrno != EAGAIN && savedErrno != EWOULDBLOCK)
        {
            handleError();
            handleClose();
        }
        return;
    }

    if (outputBuffer_.empty())
    {
        channel_->disableWriting();
        if (closeAfterWrite_)
        {
            handleClose();
        }
    }
}

void TcpConnection::handleClose()
{
    if (closed_)
    {
        return;
    }

    auto guard = shared_from_this();
    closed_ = true;
    channel_->disableAll();
    channel_->remove();
    if (closeCallback_)
    {
        closeCallback_(guard);
    }
}

void TcpConnection::handleError()
{
    std::cerr << "connection error from " << peerAddress_.toIpPort() << ", fd=" << fd() << std::endl;
}

} // namespace reactor_http_kit::net
