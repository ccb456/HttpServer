#pragma once

#include <memory>
#include <string>

namespace ccb
{
class AsyncLogger;

class Logger
{
public:
    enum class LogLevel
    {
        DEBUG = 0,
        INFO,
        WARN,
        ERROR,
        FATAL
    };

    // 初始化日志系统
    static void init(const std::string& basename = "server",
                    LogLevel level = LogLevel::DEBUG,
                    off_t rollSize = 100 * 1024 * 1024,
                    int flushInterval = 3,
                    const std::string& logDir = "logs");

    /**
     * __attribute__((format(printf, 4, 5)));
     * 这是一个 GCC/Clang 编译器属性，作用是编译期检查格式化字符串与实参是否匹配。
     * 
     * static void log(LogLevel level, const char* file, int line, const char* fmt, ...);
     *        参数1        参数2            参数3        参数4         参数5...
     * 
     * printf — 告诉编译器，格式串遵循 printf 的规则
     * 4 — 格式字符串是第 4 个参数（fmt）
     * 5 — 变参 ... 从第 5 个参数开始
     */
    static void log(LogLevel levle, const char* file, int line, const char* fmt, ...)
            __attribute__((format(printf, 4, 5)));

    static void setLogLevel(LogLevel level) { logLevel_ = level; }
    static LogLevel logLevel() { return logLevel_; }


private:
    static std::unique_ptr<AsyncLogger> asyncLogger_;
    static LogLevel logLevel_;
};

// ─── 便捷宏 ───
#define LOG_BASE(level, fmt, ...)                                          \
    do {                                                                   \
        if (static_cast<int>(level) >= static_cast<int>(ccb::Logger::logLevel())) { \
            ccb::Logger::log(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        }                                                                  \
    } while (0)


#define LOG_DEBUG(fmt, ...)  LOG_BASE(ccb::Logger::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   LOG_BASE(ccb::Logger::LogLevel::INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)   LOG_BASE(ccb::Logger::LogLevel::WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)  LOG_BASE(ccb::Logger::LogLevel::ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...)  LOG_BASE(ccb::Logger::LogLevel::FATAL, fmt, ##__VA_ARGS__)

}