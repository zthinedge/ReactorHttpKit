#include "net/Channel.h"
#include "net/EventLoop.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <sys/timerfd.h>
#include <unistd.h>

using reactor_http_kit::net::Channel;
using reactor_http_kit::net::EventLoop;

int main()
{
    int timerFd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerFd < 0)
    {
        throw std::runtime_error("timerfd_create() failed");
    }

    itimerspec spec {};
    spec.it_value.tv_sec = 1;
    spec.it_interval.tv_sec = 1;
    if (::timerfd_settime(timerFd, 0, &spec, nullptr) < 0)
    {
        ::close(timerFd);
        throw std::runtime_error("timerfd_settime() failed");
    }

    EventLoop loop;
    Channel timerChannel(&loop, timerFd);

    int ticks = 0;
    timerChannel.setReadCallback([&]() {
        uint64_t expirations = 0;
        (void)::read(timerFd, &expirations, sizeof(expirations));

        ++ticks;
        std::cout << "tick " << ticks << std::endl;
        if (ticks >= 5)
        {
            timerChannel.remove();
            loop.quit();
        }
    });
    timerChannel.enableReading();

    loop.loop();
    ::close(timerFd);
    return 0;
}
