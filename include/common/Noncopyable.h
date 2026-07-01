#pragma once

namespace ccb
{
class Noncopyable
{
protected:
    Noncopyable() = default;
    ~Noncopyable() = default;
public:
    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator= (const Noncopyable&) = delete; 
};
}
