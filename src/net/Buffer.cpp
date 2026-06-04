#include "net/Buffer.h"

#include <algorithm>
#include <cerrno>
#include <sys/uio.h>
#include <unistd.h>

namespace reactor_http_kit::net
{

size_t Buffer::readableBytes() const
{
    return buffer_.size();
}

bool Buffer::empty() const
{
    return buffer_.empty();
}

const char* Buffer::peek() const
{
    return buffer_.data();
}

std::string Buffer::toString() const
{
    return std::string(buffer_.begin(), buffer_.end());
}

void Buffer::append(const char* data, size_t length)
{
    buffer_.insert(buffer_.end(), data, data + length);
}

void Buffer::append(const std::string& data)
{
    append(data.data(), data.size());
}

void Buffer::retrieve(size_t length)
{
    if (length >= buffer_.size())
    {
        retrieveAll();
        return;
    }
    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(length));
}

void Buffer::retrieveAll()
{
    buffer_.clear();
}

ssize_t Buffer::readFd(int fd, int* savedErrno)
{
    char extraBuffer[65536];
    ssize_t n = ::read(fd, extraBuffer, sizeof(extraBuffer));
    if (n > 0)
    {
        append(extraBuffer, static_cast<size_t>(n));
    }
    else if (n < 0 && savedErrno != nullptr)
    {
        *savedErrno = errno;
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int* savedErrno)
{
    if (buffer_.empty())
    {
        return 0;
    }

    ssize_t n = ::write(fd, buffer_.data(), buffer_.size());
    if (n > 0)
    {
        retrieve(static_cast<size_t>(n));
    }
    else if (n < 0 && savedErrno != nullptr)
    {
        *savedErrno = errno;
    }
    return n;
}

} // namespace reactor_http_kit::net
