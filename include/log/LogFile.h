#pragma once

#include <string>

namespace ccb
{
class LogFile
{
public:
    LogFile(const std::string& basename, off_t rollSize = 100 * 1024 * 1024, const std::string& logDir = "logs");
    ~LogFile();

    void append(const char* data, size_t len);
    void flush();

private:
    void rollFile();
    std::string getFileName();
    void writeToFile(const char* data, size_t len);

private:
    std::string basename_;
    std::string logDir_;
    off_t rollSize_;
    off_t writtenBytes_;
    int fd_;

    time_t lastPeriod_;     // 上次写入的日期(用于按天滚动)

};
}