#pragma once

#include "common/Noncopyable.h"
#include "net/Socket.h"
#include "net/InetAddress.h"

namespace ccb
{

class Acceptor : private Noncopyable
{
public:
    explicit Acceptor(const InetAddress& addr);
    ~Acceptor() = default;

    void listen();
    int accept(InetAddress* peerAddr);

    int fd() const { return listenSocket_.fd();}


private:
    Socket listenSocket_;
    InetAddress addr_;
};
}