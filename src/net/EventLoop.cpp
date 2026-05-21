#include "net/EventLoop.h"

#include "net/Channel.h"

namespace reactor_http_kit::net
{

EventLoop::EventLoop()
    : epoll_(std::make_unique<Epoll>())
{
}

EventLoop::~EventLoop() = default;

void EventLoop::loop()
{
    quit_.store(false);
    while (!quit_.load())
    {
        auto activeChannels = epoll_->poll(10000);
        for (Channel* channel : activeChannels)
        {
            channel->handleEvent();
        }
    }
}

void EventLoop::quit()
{
    quit_.store(true);
}

void EventLoop::updateChannel(Channel* channel)
{
    epoll_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
    epoll_->removeChannel(channel);
}

} // namespace reactor_http_kit::net
