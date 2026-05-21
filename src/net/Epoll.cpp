#include "net/Epoll.h"

#include "net/Channel.h"

#include <cerrno>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace reactor_http_kit::net
{

namespace
{

constexpr int kInitialEventSize = 16;

std::runtime_error makeEpollError(const char* operation)
{
    return std::runtime_error(std::string(operation) + " failed, errno=" + std::to_string(errno));
}

} // namespace

Epoll::Epoll()
    : epollFd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitialEventSize)
{
    if (epollFd_ < 0)
    {
        throw makeEpollError("epoll_create1()");
    }
}

Epoll::~Epoll()
{
    if (epollFd_ >= 0)
    {
        ::close(epollFd_);
    }
}

std::vector<Channel*> Epoll::poll(int timeoutMs)
{
    std::vector<Channel*> activeChannels;
    int ready = ::epoll_wait(epollFd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
    if (ready < 0)
    {
        if (errno == EINTR)
        {
            return activeChannels;
        }
        throw makeEpollError("epoll_wait()");
    }

    activeChannels.reserve(static_cast<size_t>(ready));
    for (int i = 0; i < ready; ++i)
    {
        auto* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->setRevents(events_[i].events);
        activeChannels.push_back(channel);
    }

    if (ready == static_cast<int>(events_.size()))
    {
        events_.resize(events_.size() * 2);
    }

    return activeChannels;
}

void Epoll::updateChannel(Channel* channel)
{
    epoll_event event {};
    event.events = channel->events();
    event.data.ptr = channel;

    if (channel->isInEpoll())
    {
        if (::epoll_ctl(epollFd_, EPOLL_CTL_MOD, channel->fd(), &event) < 0)
        {
            throw makeEpollError("epoll_ctl(EPOLL_CTL_MOD)");
        }
        return;
    }

    if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, channel->fd(), &event) < 0)
    {
        throw makeEpollError("epoll_ctl(EPOLL_CTL_ADD)");
    }
    channel->setInEpoll(true);
}

void Epoll::removeChannel(Channel* channel)
{
    if (!channel->isInEpoll())
    {
        return;
    }

    if (::epoll_ctl(epollFd_, EPOLL_CTL_DEL, channel->fd(), nullptr) < 0)
    {
        throw makeEpollError("epoll_ctl(EPOLL_CTL_DEL)");
    }
    channel->setInEpoll(false);
}

} // namespace reactor_http_kit::net
