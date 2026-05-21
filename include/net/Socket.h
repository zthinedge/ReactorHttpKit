#pragma once

#include "InetAddress.h"

#include <cstdint>
#include <string>

namespace reactor_http_kit::net
{

int createNonblockingSocket();

class Socket
{
public:
    explicit Socket(int fd);
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int fd() const;
    std::string ip() const;
    uint16_t port() const;

    void setreuseaddr(bool on);
    void setreuseport(bool on);
    void settcpnodelay(bool on);
    void setkeepalive(bool on);

    void bind(const InetAddress& serverAddress);
    void listen(int backlog = 128);
    int accept(InetAddress& clientAddress);
    void setIpPort(const std::string& ip, uint16_t port);

private:
    int fd_ { -1 };
    std::string ip_;
    uint16_t port_ { 0 };
};

} // namespace reactor_http_kit::net
