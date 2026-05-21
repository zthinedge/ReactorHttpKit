#pragma once

#include <sys/epoll.h>

#include <vector>

namespace reactor_http_kit::net
{

class Channel;

class Epoll
{
public:
    Epoll();
    ~Epoll();

    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;

    std::vector<Channel*> poll(int timeoutMs);
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

private:
    int epollFd_ { -1 };
    std::vector<epoll_event> events_;
};

} // namespace reactor_http_kit::net
