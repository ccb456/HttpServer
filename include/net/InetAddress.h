#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

namespace ccb
{

class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false);
    InetAddress(const std::string& ip, uint16_t port);
    InetAddress(const sockaddr_in& addr);

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t port() const { return ntohs(addr_.sin_port);}

    const sockaddr_in* getSockAddr() const {return &addr_;}
    void setSockAddr(const sockaddr_in& addr) { addr_ = addr;}
    sa_family_t family()const { return addr_.sin_family;}

private:
    sockaddr_in addr_;
};
}