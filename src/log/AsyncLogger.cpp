#include "log/AsyncLogger.h"

#include <algorithm>
#include <vector>

namespace ccb
{
AsyncLogger::AsyncLogger(const std::string& basename, off_t rollSize, int flushInterval, const std::string& logDir)
                : basename_(basename), rollSize_(rollSize), flushInterval_(flushInterval),
                running_(false), logFile_(basename, rollSize, logDir)
{
}

AsyncLogger::~AsyncLogger()
{
    if(running_)
    {
        stop();
    }
}

// 启动后台线程
void AsyncLogger::start()
{
    running_ = true;
    thread_ = std::thread(&AsyncLogger::run, this);
}

// 停止后台线程
void AsyncLogger::stop()
{
    running_ = false;
    queue_.stop();
    if(thread_.joinable())
    {
        thread_.join();
    }
}

// 前端调用，将日志数据写入队列
void AsyncLogger::append(const char* data, size_t len)
{
    queue_.push(std::string(data, len));
}

// 后台线程主函数  
void AsyncLogger::run()     
{
    std::vector<std::string> buffer;
    buffer.reserve(64);

    while(running_)
    {
        buffer.clear();

        // 阻塞等待至少一条日志
        std::string first;

        // 带超时等待：flushInterval 秒内无日志也要 flush，保证落盘
        if(queue_.pop(first, flushInterval_ * 1000))
        {
            buffer.push_back(std::move(first));
        }

        // 有数据 → 非阻塞尽量多取（批量减少系统调用）
        std::string more;
        while(buffer.size() < 64 && queue_.tryPop(more))
        {
            buffer.push_back(std::move(more));
        }

        // 批量写入文件
        for(const auto& msg : buffer)
        {
            logFile_.append(msg.data(), msg.size());
        }

        // 每次循环都 flush
        // - 有日志时 ⇒ 及时落盘
        // - 超时时 ⇒ 周期性 flush（保证即使日志空闲，文件也是最新的）
        logFile_.flush();
    }

    // 停止后冲刷剩余日志
    std::string remaining;
    while(queue_.tryPop(remaining))
    {
        logFile_.append(remaining.data(), remaining.size());
    }

    logFile_.flush();
}
}