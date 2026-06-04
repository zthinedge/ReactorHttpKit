#pragma once

#include <string>
#include <sys/types.h>
#include <vector>

namespace reactor_http_kit::net
{

class Buffer
{
public:
    size_t readableBytes() const;
    bool empty() const;

    const char* peek() const;
    std::string toString() const;

    void append(const char* data, size_t length);
    void append(const std::string& data);
    void retrieve(size_t length);
    void retrieveAll();

    ssize_t readFd(int fd, int* savedErrno);
    ssize_t writeFd(int fd, int* savedErrno);

private:
    std::vector<char> buffer_;
};

} // namespace reactor_http_kit::net
