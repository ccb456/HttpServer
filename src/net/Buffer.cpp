#include "net/Buffer.h"

#include <cstring>
#include <sys/uio.h>
#include <unistd.h>
#include <cassert>


namespace ccb
{

Buffer::Buffer(const int initBufSize)
    :buffer_(initBufSize), writerIndex_(0), readerIndex_(0)
{}

size_t Buffer::writableBytes() const
{
    return buffer_.size() - writerIndex_;
}
size_t Buffer::readableBytes() const
{
    return writerIndex_ - readerIndex_;
}
size_t Buffer::prependableBytes() const
{
    return readerIndex_;
}

const char* Buffer::peek() const 
{
    return beginPtr_() + readerIndex_; 
}

void Buffer::ensureWritableBytes(size_t len)
{
    if(writableBytes() < len)
    {
        makeSpace_(len);
    }
}

void Buffer::hasWrittenBytes(size_t len)
{
    writerIndex_ += len;
}

void Buffer::retrieve(size_t len)
{
    checkInvariants();
    if(len < readableBytes())
    {
        readerIndex_ += len;
    }
    else
    {
        retrieveAll();
    }

    checkInvariants();
}

void Buffer::retrieveUntil(const char* end)
{
    assert(peek() <= end);
    retrieve(end - peek());
}

void Buffer::retrieveAll()
{
    readerIndex_ = 0;
    writerIndex_ = 0;
}

std::string Buffer::retrieveAllToString()
{
    std::string str(peek(), readableBytes());
    readerIndex_ = 0;
    writerIndex_ = 0;

    return str;
}

const char* Buffer::beginWriteConst() const
{
    return beginPtr_() + writerIndex_;
}

char* Buffer::beginWrite()
{
    return beginPtr_() + writerIndex_;
}

void Buffer::append(const std::string& str)
{
    append(str.data(), str.size());
}

void Buffer::append(const char* str, size_t len)
{
    checkInvariants();
    ensureWritableBytes(len);
    std::copy(str, str + len, beginWrite());
    hasWrittenBytes(len);
    checkInvariants();
}

void Buffer::append(const void* data, size_t len)
{
    append(static_cast<const char*>(data), len);
}

void Buffer::append(const Buffer& buffer)
{
    append(buffer.peek(), buffer.readableBytes());
}

ssize_t Buffer::readFd(int fd, int* error)
{
    char buff[65536];
    struct iovec iov[2];
    
    iov[0].iov_base = beginPtr_() + writerIndex_;
    const size_t writeBytes = writableBytes();
    iov[0].iov_len = writeBytes;

    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);

    if(len < 0)
    {
        *error = errno;
    }
    else if(static_cast<size_t>(len) <= writeBytes)
    {
        writerIndex_ += len;
    }
    else
    {
        writerIndex_ = buffer_.size();
        append(buff, len - writeBytes);
    }

    checkInvariants();

    return len;
}

ssize_t Buffer::writeFd(int fd, int* error)
{
    size_t readSize = readableBytes();
    ssize_t len = write(fd, peek(), readSize);

    if(len < 0)
    {
        *error = errno;
    }
    else if(len > 0)
    {
        readerIndex_ += len;
    }

    checkInvariants();
    return len;
}


char* Buffer::beginPtr_()
{
    return buffer_.data();
}

const char* Buffer::beginPtr_() const
{
    return buffer_.data();
}


/**
 * 调整空间函数，实现功能：
 * 1. 当空间不足时，就扩充空间。
 * 2. 当空间充足时，把可读数据移动到缓冲区起始位置。
 */
void Buffer::makeSpace_(size_t len)
{
    // 空间不足，进行扩充
    if(writableBytes() + prependableBytes() < len)
    {
        buffer_.resize(writerIndex_ + len);
        checkInvariants();
    }
    else
    {
        // 空间充足
        size_t readable = readableBytes();
        std::copy(beginPtr_() + readerIndex_, beginPtr_() + writerIndex_, beginPtr_());

        readerIndex_ = 0;
        writerIndex_ = readerIndex_ + readable;
    }

    
}

void Buffer::checkInvariants() const
{
    assert(readerIndex_ <= writerIndex_);
    assert(writerIndex_ <= buffer_.size());
}

}