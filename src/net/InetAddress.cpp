#include "net/InetAddress.h"
#include <cstring>

static const in_addr_t kInaddrAny = INADDR_ANY;
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;

namespace ccb
{
InetAddress::InetAddress(uint16_t port, bool loopbackOnly)
{
    ::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = htonl(loopbackOnly ? kInaddrLoopback : kInaddrAny);
}

InetAddress::InetAddress(const std::string& ip, uint16_t port)
{
    ::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    ::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
}

InetAddress::InetAddress(const sockaddr_in& addr)
    :addr_(addr) 
{}

std::string InetAddress::toIp() const
{
    char buf[64] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return buf;
}

std::string InetAddress::toIpPort() const
{
    char buf[64] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    snprintf(buf + end, sizeof(buf) - end, ":%u", port);
    return buf;
}

}