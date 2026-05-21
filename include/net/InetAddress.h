#pragma once

#include <cstdint>
#include <netinet/in.h>
#include <string>

namespace reactor_http_kit::net
{

class InetAddress
{
public:
    InetAddress();
    InetAddress(const std::string& ip, uint16_t port);
    explicit InetAddress(const sockaddr_in& addr);

    std::string ip() const;
    uint16_t port() const;
    std::string toIpPort() const;

    const sockaddr* addr() const;
    socklen_t addrLength() const;
    const sockaddr_in& rawAddress() const;
    void setAddress(const sockaddr_in& addr);

private:
    sockaddr_in addr_ {};
};

} // namespace reactor_http_kit::net
