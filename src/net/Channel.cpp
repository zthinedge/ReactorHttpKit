#include "net/Channel.h"

#include "net/EventLoop.h"

#include <sys/epoll.h>

namespace reactor_http_kit::net
{

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop)
    , fd_(fd)
{
}

int Channel::fd() const
{
    return fd_;
}

uint32_t Channel::events() const
{
    return events_;
}

uint32_t Channel::revents() const
{
    return revents_;
}

bool Channel::isInEpoll() const
{
    return inEpoll_;
}

void Channel::setRevents(uint32_t revents)
{
    revents_ = revents;
}

void Channel::setInEpoll(bool inEpoll)
{
    inEpoll_ = inEpoll;
}

void Channel::enableReading()
{
    events_ |= (EPOLLIN | EPOLLPRI);
    update();
}

void Channel::disableReading()
{
    events_ &= ~(EPOLLIN | EPOLLPRI);
    update();
}

void Channel::enableWriting()
{
    events_ |= EPOLLOUT;
    update();
}

void Channel::disableWriting()
{
    events_ &= ~EPOLLOUT;
    update();
}

void Channel::disableAll()
{
    events_ = 0;
    update();
}

void Channel::remove()
{
    if (loop_ != nullptr)
    {
        loop_->removeChannel(this);
    }
}

void Channel::handleEvent()
{
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
        return;
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
    {
        if (readCallback_)
        {
            readCallback_();
        }
    }

    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}

void Channel::setReadCallback(EventCallback cb)
{
    readCallback_ = std::move(cb);
}

void Channel::setWriteCallback(EventCallback cb)
{
    writeCallback_ = std::move(cb);
}

void Channel::setCloseCallback(EventCallback cb)
{
    closeCallback_ = std::move(cb);
}

void Channel::setErrorCallback(EventCallback cb)
{
    errorCallback_ = std::move(cb);
}

void Channel::update()
{
    if (loop_ != nullptr)
    {
        loop_->updateChannel(this);
    }
}

} // namespace reactor_http_kit::net
