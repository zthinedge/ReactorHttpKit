#include "net/InetAddress.h"

#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>

namespace reactor_http_kit::net
{

InetAddress::InetAddress()
{
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_.sin_port = htons(0);
}

InetAddress::InetAddress(const std::string& ip, uint16_t port)
{
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);

    if (::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0)
    {
        throw std::invalid_argument("invalid IPv4 address: " + ip);
    }
}

InetAddress::InetAddress(const sockaddr_in& addr)
    : addr_(addr)
{
}

std::string InetAddress::ip() const
{
    char buffer[INET_ADDRSTRLEN] = {};
    if (::inet_ntop(AF_INET, &addr_.sin_addr, buffer, sizeof(buffer)) == nullptr)
    {
        return {};
    }
    return buffer;
}

uint16_t InetAddress::port() const
{
    return ntohs(addr_.sin_port);
}

std::string InetAddress::toIpPort() const
{
    return ip() + ":" + std::to_string(port());
}

const sockaddr* InetAddress::addr() const
{
    return reinterpret_cast<const sockaddr*>(&addr_);
}

socklen_t InetAddress::addrLength() const
{
    return static_cast<socklen_t>(sizeof(addr_));
}

const sockaddr_in& InetAddress::rawAddress() const
{
    return addr_;
}

void InetAddress::setAddress(const sockaddr_in& addr)
{
    addr_ = addr;
}

} // namespace reactor_http_kit::net
