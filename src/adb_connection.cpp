// src/adb_connection.cpp

#include "adb_connection.hpp"

#include <arpa/inet.h>
#include <cstdio>
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
    inet_pton(AF_INET, kAdbHost, &addr.sin_addr);

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
    if (fd_ < 0)
        return false;

    char header[5];
    unsigned int len = static_cast<unsigned int>(payload.size() & 0xffffu);
    std::snprintf(header, sizeof(header), "%04x", len);

    if (write(fd_, header, 4) != 4)
        return false;
    if (write(fd_, payload.data(), payload.size()) != static_cast<ssize_t>(payload.size()))
        return false;
    return true;
}

Status Connection::read_status(std::string &err_message)
{
    if (fd_ < 0)
        return Status::IoError;

    char status[4];
    if (read(fd_, status, 4) != 4)
        return Status::IoError;

    if (std::memcmp(status, "OKAY", 4) == 0)
        return Status::Okay;

    if (std::memcmp(status, "FAIL", 4) == 0)
    {
        char lenhex[5] = {0};
        if (read(fd_, lenhex, 4) == 4)
        {
            long msglen = std::strtol(lenhex, nullptr, 16);
            if (msglen > 0)
            {
                std::string buf(static_cast<size_t>(msglen), '\0');
                ssize_t n = read(fd_, buf.data(), buf.size());
                if (n > 0)
                {
                    buf.resize(static_cast<size_t>(n));
                    err_message = buf;
                }
            }
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
    if (read(fd_, lenhex, 4) != 4)
        return {};

    long msglen = std::strtol(lenhex, nullptr, 16);
    if (msglen < 0)
        return {};

    std::string buf(static_cast<size_t>(msglen), '\0');
    size_t total = 0;
    while (total < buf.size())
    {
        ssize_t n = read(fd_, buf.data() + total, buf.size() - total);
        if (n <= 0)
            break;
        total += static_cast<size_t>(n);
    }
    buf.resize(total);
    return buf;
}

} // namespace adb
