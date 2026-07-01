#include "net/Epoller.h"

#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

namespace ccb
{

Epoller::Epoller(int maxEvents)
    :epollfd_(epoll_create(512)),
    events_(maxEvents)
{
    assert(epollfd_ > 0 && events_.size() > 0);
}
Epoller::~Epoller()
{
    close(epollfd_);
}

bool Epoller::addFd(int fd, uint32_t events)
{
    if(fd < 0) return false;

    epoll_event ev{0};
    ev.data.fd = fd;
    ev.events = events;

    return 0 == epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::modFd(int fd, uint32_t events)
{
    if(fd < 0) return false;

    epoll_event ev{0};
    ev.data.fd = fd;
    ev.events = events;

    return 0 == epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::delFd(int fd)
{
    if(fd < 0) return false;

    // epoll_event ev{0};

    return 0 == epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, nullptr);
}

int Epoller::wait(int timeoutMs)
{
    return epoll_wait(epollfd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

int Epoller::getEventFd(size_t idx) const
{
    assert(idx < events_.size() && idx >= 0);
    return events_[idx].data.fd;
}

uint32_t Epoller::getEvents(size_t idx) const
{
    assert(idx < events_.size() && idx >= 0);
    return events_[idx].events;
}

}