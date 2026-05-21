#pragma once

#include "net/Epoll.h"

#include <atomic>
#include <memory>

namespace reactor_http_kit::net
{

class Channel;

class EventLoop
{
public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void loop();
    void quit();

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

private:
    std::unique_ptr<Epoll> epoll_;
    std::atomic_bool quit_ { false };
};

} // namespace reactor_http_kit::net
