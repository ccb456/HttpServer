#pragma once

#include <sys/epoll.h>
#include <vector>

namespace ccb 
{

class Epoller
{

public:
    explicit Epoller(int maxEvents = 1024);
    ~Epoller();

    bool addFd(int fd, uint32_t events);
    bool modFd(int fd, uint32_t events);
    bool delFd(int fd);

    int wait(int timeoutMs = -1);
    int getEventFd(size_t idx) const;
    uint32_t getEvents(size_t idx) const;

private:
    int epollfd_;
    std::vector<epoll_event> events_;
};    

}
