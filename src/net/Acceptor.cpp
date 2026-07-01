#include "net/Acceptor.h"
#include "log/Logger.h"

namespace ccb
{
Acceptor::Acceptor(const InetAddress& addr)
    :listenSocket_(::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)),
    addr_(addr)
    
{
    listenSocket_.setReuseAddr(true);
    listenSocket_.setReusePort(true);
    listenSocket_.bind(addr_);
    LOG_INFO("Acceptor bound on %s", addr_.toIpPort().c_str());
}

void Acceptor::listen()
{
    listenSocket_.listen();
}

int Acceptor::accept(InetAddress* peerAddr)
{
    return listenSocket_.accept(peerAddr);
}

}