#include "net/Socket.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace reactor_http_kit::net
{

int createNonblockingSocket()
{
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (fd < 0)
    {
        std::perror("socket() failed");
        std::exit(EXIT_FAILURE);
    }
    return fd;
}

Socket::Socket(int fd)
    : fd_(fd)
{
}

Socket::~Socket()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
    }
}

int Socket::fd() const
{
    return fd_;
}

std::string Socket::ip() const
{
    return ip_;
}

uint16_t Socket::port() const
{
    return port_;
}

void Socket::setreuseaddr(bool on)
{
    int opt = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, static_cast<socklen_t>(sizeof(opt)));
}

void Socket::setreuseport(bool on)
{
    int opt = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &opt, static_cast<socklen_t>(sizeof(opt)));
}

void Socket::settcpnodelay(bool on)
{
    int opt = on ? 1 : 0;
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &opt, static_cast<socklen_t>(sizeof(opt)));
}

void Socket::setkeepalive(bool on)
{
    int opt = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, static_cast<socklen_t>(sizeof(opt)));
}

void Socket::bind(const InetAddress& serverAddress)
{
    if (::bind(fd_, serverAddress.addr(), serverAddress.addrLength()) < 0)
    {
        std::perror("bind() failed");
        std::exit(EXIT_FAILURE);
    }
    setIpPort(serverAddress.ip(), serverAddress.port());
}

void Socket::listen(int backlog)
{
    if (::listen(fd_, backlog) < 0)
    {
        std::perror("listen() failed");
        std::exit(EXIT_FAILURE);
    }
}

int Socket::accept(InetAddress& clientAddress)
{
    sockaddr_in peerAddress {};
    socklen_t length = static_cast<socklen_t>(sizeof(peerAddress));
    int clientFd = ::accept4(fd_, reinterpret_cast<sockaddr*>(&peerAddress), &length,
                            SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (clientFd >= 0)
    {
        clientAddress.setAddress(peerAddress);
    }
    return clientFd;
}

void Socket::setIpPort(const std::string& ip, uint16_t port)
{
    ip_ = ip;
    port_ = port;
}

} // namespace reactor_http_kit::net
