#pragma once

#include "common/Noncopyable.h"

namespace ccb
{
class InetAddress;

class Socket : private Noncopyable
{
public:
    explicit Socket(int fd);
    Socket(int domain, int type, int protocol = 0);
    ~Socket();

    Socket(Socket&& other) noexcept : fd_(other.fd_){ other.fd_ = -1;}

    int fd() const { return fd_;}

    void bind(const InetAddress& addr);
    void listen(int backlog = 128);
    int accept(InetAddress* peerAddr);

    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setTcpNoDelay(bool on);
    void setKeepAlive(bool on);
    void setNonBlocking(bool on);

    void shutdownWrite();


private:
    int fd_;
};
}