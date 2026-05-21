#pragma once

#include <cstdint>
#include <functional>

namespace reactor_http_kit::net
{

class EventLoop;

class Channel
{
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel() = default;

    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    int fd() const;
    uint32_t events() const;
    uint32_t revents() const;
    bool isInEpoll() const;

    void setRevents(uint32_t revents);
    void setInEpoll(bool inEpoll);

    void enableReading();
    void disableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();

    void remove();
    void handleEvent();

    void setReadCallback(EventCallback cb);
    void setWriteCallback(EventCallback cb);
    void setCloseCallback(EventCallback cb);
    void setErrorCallback(EventCallback cb);

private:
    void update();

private:
    EventLoop* loop_ { nullptr };
    int fd_ { -1 };
    uint32_t events_ { 0 };
    uint32_t revents_ { 0 };
    bool inEpoll_ { false };

    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

} // namespace reactor_http_kit::net
