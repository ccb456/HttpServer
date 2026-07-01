#include "log/LogFile.h"

#include <cstdio>
#include <ctime>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace ccb
{

LogFile::LogFile(const std::string& basename, off_t rollSize, const std::string& logDir)
    : basename_(basename),
      logDir_(logDir),
      rollSize_(rollSize),
      writtenBytes_(0),
      fd_(-1),
      lastPeriod_(0)
{
    rollFile();
}

LogFile::~LogFile()
{
    if(fd_ > 0)
    {
        ::close(fd_);
    }
}

void LogFile::append(const char* data, size_t len)
{
    if(fd_ < 0) return;

    // 按天数滚动
    time_t now = ::time(nullptr);
    time_t thisPeriod = now / 86400;    // 今天的起始
    if(thisPeriod != lastPeriod_)
    {
        rollFile();
    }

    // 按大小滚动
    if(writtenBytes_ + static_cast<off_t>(len) > rollSize_)
    {
        rollFile();
    }

    writeToFile(data, len);
}

void LogFile::flush()
{
    if(fd_ > 0)
    {
        ::fsync(fd_);
    }
}

void LogFile::rollFile()
{
    if(fd_ >= 0)
    {
        ::close(fd_);
    }

    // 创建日志目录
    ::mkdir(logDir_.c_str(), 0755);

    std::string filename = logDir_ + "/" + getFileName();
    fd_ = ::open(filename.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    writtenBytes_ = 0;
    lastPeriod_ = ::time(nullptr) / 86400;
}
std::string LogFile::getFileName()
{
    time_t now = ::time(nullptr);
    struct tm tm;
    ::localtime_r(&now, &tm);

    char buf[128];
    snprintf(buf, sizeof(buf), "%s_%04d%02d%02d_%02d%02d%02d.log",
            basename_.c_str(),
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    
    return buf;
}

void LogFile::writeToFile(const char* data, size_t len)
{
    ssize_t n = ::write(fd_, data, len);
    if(n > 0)
    {
        writtenBytes_ += n;
    }
}

}