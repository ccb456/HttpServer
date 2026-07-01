#include "log/Logger.h"
#include "log/AsyncLogger.h"

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <chrono>

namespace ccb
{

std::unique_ptr<AsyncLogger> Logger::asyncLogger_ = nullptr;
Logger::LogLevel Logger::logLevel_ = Logger::LogLevel::DEBUG;

void Logger::init(const std::string& basename,
                LogLevel level,
                off_t rollSize,
                int flushInterval,
                const std::string& logDir)
{
    logLevel_ = level;
    asyncLogger_ = std::make_unique<AsyncLogger>(basename, rollSize, flushInterval, logDir);
    asyncLogger_->start();
}

void Logger::log(LogLevel level, const char* file, int line, const char* fmt, ...)
{
    if(!asyncLogger_)   return;
    static const char* levelStrs[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    static const size_t kBufSize = 4096;
    char buf[kBufSize] = {0};
    int written = 0;

    // 1.时间戳
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    time_t t = std::chrono::system_clock::to_time_t(now);

    struct tm tm;
    ::localtime_r(&t, &tm);

    written += snprintf(buf + written, kBufSize - written,
                        "[%04d-%02d-%02d %02d:%02d:%02d.%03ld] [%s] [%s:%d] ",
                        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                        tm.tm_hour, tm.tm_min, tm.tm_sec,
                        static_cast<long>(ms.count()),
                        levelStrs[static_cast<int>(level)], file, line);

    if(written < 0) written = 0;
    if(static_cast<int>(written) >= kBufSize)
    {
        written = kBufSize - 1;
    }
    
    // 用户消息 — 预留 2 字节给 '\n' 和 '\0'
    va_list args;
    va_start(args, fmt);

    int n = vsnprintf(buf + written, kBufSize - written - 2, fmt,  args);
    va_end(args);

    if(n > 0)
    {
        written += n;
    }

    // 换行
    buf[written++] = '\n';
    buf[written] = '\0';

    asyncLogger_->append(buf, written);

}
}