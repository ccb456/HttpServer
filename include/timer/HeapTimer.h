#pragma once

#include <vector>
#include <unordered_map>
#include <functional>

namespace ccb
{

using TimeoutCallback = std::function<void()>;

class HeapTimer
{

public:
    HeapTimer() = default;
    ~HeapTimer() { clear();}

    void add(int id, int timeoutMs, const TimeoutCallback& cb);
    void adjust(int id, int newTimeoutMs);
    void del(int id);

    void tick();
    int getNextTick();

    void clear();
    bool empty() const;
    size_t size() const;

private:
    void shiftUp(size_t idx);
    void shiftDown(size_t idx);
    void swapNode(size_t i, size_t j);
    void del_(size_t idx);
    

private:
    struct TimerNode
    {
        int id;
        int64_t timeout;
        TimeoutCallback cb;
    };

    std::vector<TimerNode> heap_;
    std::unordered_map<int, size_t> pos_;
};
}