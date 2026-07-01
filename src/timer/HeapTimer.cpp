#include "timer/HeapTimer.h"

#include <chrono>
#include <cassert>
#include <climits>

namespace ccb
{
static int64_t getCurMs()
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

void HeapTimer::add(int id, int timeoutMs, const TimeoutCallback& cb)
{
    auto it = pos_.find(id);
    if(it != pos_.end())
    {
        // 已有该定时器->更新超时时间和回调
        size_t idx = it->second;
        heap_[idx].timeout = getCurMs() + timeoutMs;
        heap_[idx].cb = cb;

        shiftDown(idx);
        shiftUp(idx);
    }
    else
    {
        size_t idx = heap_.size();
        pos_[id] = idx;
        heap_.push_back({id, getCurMs() + timeoutMs, cb});
        shiftUp(idx);
    }
}

void HeapTimer::adjust(int id, int newTimeoutMs)
{
    auto it = pos_.find(id);
    if(it == pos_.end()) return;

    size_t idx = it->second;
    heap_[idx].timeout = getCurMs() + newTimeoutMs;
    shiftDown(idx);
    shiftUp(idx);

}

void HeapTimer::del(int id)
{
    auto it = pos_.find(id);
    if(it == pos_.end()) return;

    del_(it->second);
}

void HeapTimer::tick()
{
    int64_t now = getCurMs();
    while(!heap_.empty() && heap_[0].timeout <= now)
    {
        TimerNode node = heap_[0];
        del_(0);
        if(node.cb) node.cb();
    }
}

int HeapTimer::getNextTick()
{
    if(heap_.empty()) return -1;
    int64_t now = getCurMs();
    int64_t diff = heap_[0].timeout - now;
    if(diff <= 0) return 0;

    return diff > INT_MAX ? -1 : static_cast<int>(diff);
}

void HeapTimer::clear()
{
    heap_.clear();
    pos_.clear();
}

bool HeapTimer::empty() const
{
    return heap_.empty();
}

size_t HeapTimer::size() const
{
    return heap_.size();
}

void HeapTimer::shiftUp(size_t idx)
{
    if(idx == 0) return;
    size_t parent = (idx - 1) / 2;
    if(heap_[idx].timeout < heap_[parent].timeout)
    {
        swapNode(idx, parent);
        shiftUp(parent);
    }
}

void HeapTimer::shiftDown(size_t idx)
{
    size_t left = idx * 2 + 1;
    size_t right = idx * 2 + 2;
    size_t smallest = idx;

    if(left < heap_.size() && heap_[left].timeout < heap_[smallest].timeout)
    {
        smallest = left;
    }

    if(right < heap_.size() && heap_[right].timeout < heap_[smallest].timeout)
    {
        smallest = right;
    }

    if(smallest != idx)
    {
        swapNode(idx, smallest);
        shiftDown(smallest);
    }
}

void HeapTimer::swapNode(size_t i, size_t j)
{
    std::swap(heap_[i], heap_[j]);
    pos_[heap_[i].id] = i;
    pos_[heap_[j].id] = j;
}

void HeapTimer::del_(size_t idx)
{
    assert(!heap_.empty() && idx < heap_.size());
    size_t lastIdx = heap_.size() - 1;
    if(idx < lastIdx)
    {
        swapNode(idx, lastIdx);
        pos_.erase(heap_[lastIdx].id);
        heap_.pop_back();
        shiftDown(idx);
        shiftUp(idx);
    }
    else
    {
        pos_.erase(heap_[idx].id);
        heap_.pop_back();
    }
}

}