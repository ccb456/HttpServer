#pragma once

#include "common/Noncopyable.h"
#include "log/BlockQueue.h"
#include "log/LogFile.h"

#include <thread>
#include <atomic>
#include <string>

namespace ccb
{
class AsyncLogger : private Noncopyable
{
public:
    AsyncLogger(const std::string& basename, 
                off_t rollSize = 100 * 1024 * 1024, 
                int flushInterval = 3,
                const std::string& logDir = "logs");
    ~AsyncLogger();

    // 启动后台线程
    void start();

    // 停止后台线程
    void stop();

    // 前端调用，将日志数据写入队列
    void append(const char* data, size_t len);

private:
    void run();     // 后台线程主函数

private:
    std::string basename_;
    off_t rollSize_;
    int flushInterval_;         // 冲刷间隔(秒)

    BlockQueue<std::string> queue_;
    std::thread thread_;
    std::atomic_bool running_;
    LogFile logFile_;

};
}