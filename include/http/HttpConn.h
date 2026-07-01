#pragma once

#include "common/Noncopyable.h"
#include "net/Buffer.h"
#include "net/InetAddress.h"
#include "http/HttpParser.h"
#include "http/HttpResponse.h"

#include <string>

namespace ccb
{
class HttpConn : private Noncopyable
{
public:
    explicit HttpConn(int fd);
    ~HttpConn();

    /// 从 fd 读取数据到读缓冲，返回读取的字节数（<=0 表示出错或关闭）
    ssize_t read();
    /// 将写缓冲数据写入 fd，返回已写字节数
    ssize_t write();
    /// 解析读缓冲并生成响应到写缓冲，返回 true 表示完成
    bool process();

    int fd() const { return fd_; }
    bool isKeepAlive() const { return parser_.request().isKeepAlive();}
    bool isClosed() const { return closed_; }

    // 是否还有待发送数据（缓冲区 或 文件）
    bool hasMoreToWrite() const { return writeBuf_.readableBytes() > 0 || fileRemaining_ > 0; }

    void setClosed(bool c) { closed_ = c; }
    void setPeerAddr(const InetAddress& addr) { peerAddr_ = addr; }
    const InetAddress peerAddr() const { return peerAddr_;}

    // 设置静态文件根目录
    static void setSrcDir(const std::string& dir){ srcDir_ = dir; }
    static const std::string& srcDir() { return srcDir_; }

private:
    int fd_;
    bool closed_;

    Buffer readBuf_;    // 读缓冲
    Buffer writeBuf_;   // 写缓冲（只存 HTTP 头部，文件体用 sendfile）

    HttpParser parser_;
    HttpResponse response_;
    InetAddress peerAddr_;

    // sendfile 相关：大文件零拷贝发送
    int fileFd_ = -1;
    off_t fileOffset_ = 0;
    size_t fileRemaining_ = 0;

    static std::string srcDir_;
};

}