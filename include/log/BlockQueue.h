#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace ccb
{
template <typename T>
class BlockQueue
{
public:

    explicit BlockQueue(size_t maxCapacity = 1024)
        : capacity_(maxCapacity), stopped_(false)
    {

    }

    void push(const T& item)
    {
        std::unique_lock<std::mutex> lock(mtx_);
        notFull_.wait(lock, [this]() {
            return queue_.size() < capacity_ || stopped_;
        });

        if(stopped_)    return;
        queue_.push_back(item);
        notEmpty_.notify_one();
    }

    void push(T&& item)
    {
        std::unique_lock<std::mutex> lock(mtx_);
        notFull_.wait(lock, [this]() {
            return queue_.size() < capacity_ || stopped_;
        });

        if(stopped_)    return;
        queue_.push_back(std::move(item));
        notEmpty_.notify_one();
    }

    bool pop(T& item)
    {
        return pop(item, -1);   // wait_for(-1) 等价于永久等待
    }

    /// 阻塞直到有数据或超时，超时返回 false
    bool pop(T& item, int timeoutMs) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (!notEmpty_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                [this]() { return !queue_.empty() || stopped_; })) {
            return false;  // 超时
        }
        if (stopped_ && queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop_front();
        notFull_.notify_one();
        return true;
    }

    bool tryPop(T& item)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if(queue_.empty()) return false;

        item = std::move(queue_.front());
        queue_.pop_front();
        notFull_.notify_one();

        return true;
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stopped_ = true;
        }

        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    bool full() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size() >= capacity_;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.clear();
    }

private:
    std::deque<T> queue_;
    size_t capacity_;
    mutable std::mutex mtx_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;

    bool stopped_;
};
    
}