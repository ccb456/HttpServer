#include "net/Socket.h"
#include "net/InetAddress.h"
#include "log/Logger.h"

#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ccb
{
Socket::Socket(int fd)
    : fd_(fd)
{}
Socket::Socket(int domain, int type, int protocol)
    : fd_(::socket(domain, type, protocol))
{}
Socket::~Socket()
{
    if(fd_ > 0)
    {
        ::close(fd_);
    }
}


void Socket::bind(const InetAddress& addr)
{
    const sockaddr_in& saddr = *addr.getSockAddr();
    int ret = ::bind(fd_, (sockaddr*)&saddr, sizeof(saddr));
    if(ret < 0)
    {
        LOG_FATAL("Socket::bind error fd=%d, errno=%d", fd_, errno);
        abort();
    }
}

void Socket::listen(int backlog)
{
    int ret = ::listen(fd_, backlog);
    if(ret < 0)
    {
        LOG_FATAL("Socket::listen error fd=%d, errno=%d", fd_, errno);
        abort();
    }
    LOG_DEBUG("Socket listening on fd=%d", fd_);
}

int Socket::accept(InetAddress* peerAddr)
{
    sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);

    int connFd = ::accept4(fd_, (sockaddr*)&addr, &addrLen,
                             SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connFd >= 0 && peerAddr)
    {
        peerAddr->setSockAddr(addr);
    }

    return connFd;

}

void Socket::setReuseAddr(bool on)
{
    int opt = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

void Socket::setReusePort(bool on)
{
    int opt = on ? 1 : 0;
    ::setsockopt(fd_,SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
}

void Socket::setTcpNoDelay(bool on)
{
    int opt = on ? 1 : 0;
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
}

void Socket::setKeepAlive(bool on)
{
    int opt = on ? 1 : 0;
    ::setsockopt(fd_,SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
}

void Socket::setNonBlocking(bool on)
{
    int flags = ::fcntl(fd_, F_GETFL, 0);
    flags = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    ::fcntl(fd_, F_SETFL, flags);
}

void Socket::shutdownWrite()
{
    ::shutdown(fd_, SHUT_WR);
}

}