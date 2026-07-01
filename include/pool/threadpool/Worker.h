#pragma once
#include <cstddef>
#include <thread>

#include "common/Noncopyable.h"

namespace ccb
{

class Worker : private Noncopyable
{    
public:
    Worker(std::size_t id, std::thread&& thd);
    ~Worker();

    void join();
    bool joinable() const;
    std::size_t getId() const;

private:
    std::size_t id_;
    std::thread worker_;

};

}

