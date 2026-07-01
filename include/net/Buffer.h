#pragma once

#include <vector>
#include <atomic>
#include <string>

namespace ccb
{

/// +------------------+------------------+
/// |  readable bytes  |  writable bytes  |
/// |     (CONTENT)    |                  |
/// +------------------+------------------+
/// |                  |                  |
/// readerIndex <= writerIndex   <=     size

class Buffer
{
    
public:
    explicit Buffer(const int initBufSize = 1024);
    ~Buffer() = default;

    size_t writableBytes() const;
    size_t readableBytes() const;
    size_t prependableBytes() const;    // 前部可插入的字节数

    const char* peek() const;           // 首部指针
    void ensureWritableBytes(size_t len);
    void hasWrittenBytes(size_t len);

    void retrieve(size_t len);
    void retrieveUntil(const char* end);
    void retrieveAll();
    std::string retrieveAllToString();

    const char* beginWriteConst() const;
    char* beginWrite();

    void append(const std::string& str);
    void append(const char* str, size_t len);
    void append(const void* data, size_t len);
    void append(const Buffer& buffer);

    ssize_t readFd(int fd, int* error);
    ssize_t writeFd(int fd, int* error);

private:
    char* beginPtr_();
    const char* beginPtr_() const;
    void makeSpace_(size_t len);
    void checkInvariants() const;

private:
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};
}


