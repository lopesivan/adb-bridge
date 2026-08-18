// src/adb_connection.cpp

#include "adb_connection.hpp"

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace adb
{

namespace
{
constexpr const char *kAdbHost = "127.0.0.1";
constexpr int kAdbPort = 5037;
constexpr size_t kChunk = 4096;
} // namespace

Connection::Connection() : fd_(-1)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return;

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kAdbPort);

    if (inet_pton(AF_INET, kAdbHost, &addr.sin_addr) != 1)
    {
        close(fd);
        return;
    }

    if (connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        close(fd);
        return;
    }
    fd_ = fd;
}

Connection::~Connection()
{
    if (fd_ >= 0)
        close(fd_);
}

Connection::Connection(Connection &&other) noexcept : fd_(other.fd_)
{
    other.fd_ = -1;
}

Connection &Connection::operator=(Connection &&other) noexcept
{
    if (this != &other)
    {
        if (fd_ >= 0)
            close(fd_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool Connection::send_request(const std::string &payload)
{
    if (fd_ < 0 || payload.size() > 0xffffu)
        return false;

    char header[5];
    std::snprintf(header, sizeof(header), "%04x", static_cast<unsigned int>(payload.size()));

    return write_raw(header, 4) && write_raw(payload.data(), payload.size());
}

Status Connection::read_status(std::string &err_message)
{
    err_message.clear();
    if (fd_ < 0)
        return Status::IoError;

    char status[4];
    if (!read_raw(status, sizeof(status)))
        return Status::IoError;

    if (std::memcmp(status, "OKAY", 4) == 0)
        return Status::Okay;

    if (std::memcmp(status, "FAIL", 4) == 0)
    {
        char lenhex[5] = {0};
        if (!read_raw(lenhex, 4))
            return Status::IoError;

        char *end = nullptr;
        unsigned long msglen = std::strtoul(lenhex, &end, 16);
        if (end != lenhex + 4)
            return Status::IoError;

        if (msglen > 0)
        {
            std::string buf(static_cast<size_t>(msglen), '\0');
            if (!read_raw(buf.data(), buf.size()))
                return Status::IoError;
            err_message = std::move(buf);
        }
        return Status::Fail;
    }

    return Status::IoError;
}

std::string Connection::read_until_eof()
{
    std::string result;
    if (fd_ < 0)
        return result;

    char tmp[kChunk];
    ssize_t n;
    while ((n = read(fd_, tmp, sizeof(tmp))) > 0)
        result.append(tmp, static_cast<size_t>(n));

    return result;
}

std::string Connection::read_framed()
{
    if (fd_ < 0)
        return {};

    char lenhex[5] = {0};
    if (!read_raw(lenhex, 4))
        return {};

    char *end = nullptr;
    unsigned long msglen = std::strtoul(lenhex, &end, 16);
    if (end != lenhex + 4)
        return {};

    std::string buf(static_cast<size_t>(msglen), '\0');
    if (!buf.empty() && !read_raw(buf.data(), buf.size()))
        return {};
    return buf;
}

bool Connection::write_raw(const void *data, size_t len)
{
    if (fd_ < 0)
        return false;

    size_t total = 0;
    const char *p = static_cast<const char *>(data);
    while (total < len)
    {
        ssize_t n = write(fd_, p + total, len - total);
        if (n <= 0)
            return false;
        total += static_cast<size_t>(n);
    }
    return true;
}

bool Connection::read_raw(void *buf, size_t len)
{
    if (fd_ < 0)
        return false;

    size_t total = 0;
    char *p = static_cast<char *>(buf);
    while (total < len)
    {
        ssize_t n = read(fd_, p + total, len - total);
        if (n <= 0)
            return false;
        total += static_cast<size_t>(n);
    }
    return true;
}

} // namespace adb
