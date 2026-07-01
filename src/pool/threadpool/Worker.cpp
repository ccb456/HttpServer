#include "pool/threadpool/Worker.h"

namespace ccb
{
    
Worker::Worker(std::size_t id, std::thread &&thd)
    : id_(id), worker_(std::move(thd))
{
}
Worker::~Worker()
{
    join();
}

void Worker::join()
{
    if(worker_.joinable())
    {
        worker_.join();
    }
}

bool Worker::joinable() const
{
    return worker_.joinable();
}

std::size_t Worker::getId() const
{
    return id_;
}

}

