#include "http/HttpConn.h"
#include "log/Logger.h"
#include "http/HttpHandler.h"

#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>

namespace ccb
{

std::string HttpConn::srcDir_ = "./webroot";

HttpConn::HttpConn(int fd)
    :fd_(fd), closed_(false)
{
    LOG_DEBUG("HttpConn created fd=%d", fd);
}

HttpConn::~HttpConn()
{
    if(fileFd_ >= 0)
    {
        ::close(fileFd_);
        fileFd_ = -1;
    }
    if(!closed_ && fd_ >= 0)
    {
        LOG_DEBUG("HttpConn destroyed, closing fd=%d", fd_);
        ::close(fd_);
    }
}

ssize_t HttpConn::read()
{
    int err = 0;
    ssize_t n = readBuf_.readFd(fd_, &err);
    if(n <= 0)
    {
        if(n == 0)
        {
            closed_ = true;
            LOG_INFO("Peer closed connection fd=%d", fd_);
        }
        else
        {
            LOG_ERROR("read error fd=%d, errno=%d", fd_, err);
        }
    }

    return n;
}

ssize_t HttpConn::write()
{
    // 先写完缓冲区（HTTP 头部）
    if (writeBuf_.readableBytes() > 0)
    {
        int err = 0;
        ssize_t n = writeBuf_.writeFd(fd_, &err);
        if (n < 0)
        {
            closed_ = true;
            LOG_ERROR("write error fd=%d, errno=%d", fd_, err);
            return n;
        }
        // 如果头部还没写完，返回已写字节数
        if (writeBuf_.readableBytes() > 0)
            return n;
    }

    // 头部写完后，用 sendfile 发送文件内容（零拷贝）
    if (fileFd_ >= 0 && fileRemaining_ > 0)
    {
        ssize_t sent = ::sendfile(fd_, fileFd_, &fileOffset_, fileRemaining_);
        if (sent < 0)
        {
            if (errno == EAGAIN)
                return 0;  // 写缓冲区满，下次 EPOLLOUT 再试
            closed_ = true;
            LOG_ERROR("sendfile error fd=%d, errno=%d", fd_, errno);
            return -1;
        }
        fileRemaining_ -= sent;
        if (fileRemaining_ == 0)
        {
            ::close(fileFd_);
            fileFd_ = -1;
            fileOffset_ = 0;
        }
        return sent;
    }

    return 0;
}

bool HttpConn::process()
{
    // 1. 解析HTTP请求
    parser_.reset();  // 每次处理前重置解析器状态
    auto state = parser_.parse(&readBuf_);

    if(state == HttpParser::ParseState::kParseError)
    {
        LOG_WARN("HTTP parse error fd=%d, returning 400", fd_);
        response_.reset();
        response_.setStatusCode(400);
        response_.setStatusMessage("Bad Request");
        response_.setContentType("text/html");
        response_.setBody("<html><body><h1>400 Bad Request</h1></body></html>");
        response_.setKeepAlive(false);
        response_.appendToBuffer(&writeBuf_);

        return true;
    }

    if(state != HttpParser::ParseState::kParseComplete)
    {
        LOG_DEBUG("HTTP request incomplete fd=%d, waiting for more data", fd_);
        return false;   // 数据不全，等待更多
    }

    // 2.解析完成，生成相应
    const HttpRequest& req = parser_.request();
    std::string filePath = srcDir_ + req.path();

    if(req.method() == HttpRequest::Method::kPost)
    {
        if(req.path() == "/login")
        {
            HttpHandler::handleLogin(req, response_);
        }
        else if(req.path() == "/register")
        {
            HttpHandler::handleRegister(req, response_);
        }
        else
        {
            // 未支持的 POST 路径 → 405 Method Not Allowed
            response_.setStatusCode(405);
            response_.setStatusMessage("Method Not Allowed");
            response_.setContentType("text/html");
            response_.setBody("<html><body><h1>405 Method Not Allowed</h1></body></html>");
            response_.setKeepAlive(false);
        }

        response_.addHeader("Server", "ccbWebServer/1.0");
        response_.appendToBuffer(&writeBuf_);
        return true;
    }

    // 默认首页
    if(req.path() == "/")
    {
        filePath = srcDir_ + "/index.html";
    }

    LOG_INFO("Request %s fd=%d", req.methodString().c_str(), fd_);
    LOG_DEBUG("Request path=%s fd=%d", filePath.c_str(), fd_);

    struct stat st;
    if(::stat(filePath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
    {
        // 文件存在
    }
    else
    {
        // 尝试补 .html 后缀（如 /login → /login.html）
        std::string htmlPath = filePath + ".html";
        if(::stat(htmlPath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
        {
            filePath = htmlPath;
        }
    }

    if(::stat(filePath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
    {
        // 文件存在
        response_.setStatusCode(200);
        response_.setStatusMessage("OK");

        // 推断Content-type
        size_t dot = filePath.find_last_of('.');
        if(dot != std::string::npos)
        {
            response_.setContentType(HttpResponse::getContentType(filePath.substr(dot)));
        }
        else
        {
            response_.setContentType("text/plain");
        }

        response_.setKeepAlive(req.isKeepAlive());

        // 用 sendfile 零拷贝发送文件（仅写头部到缓冲，文件体直接内核发送）
        fileFd_ = ::open(filePath.c_str(), O_RDONLY);
        if (fileFd_ >= 0)
        {
            fileOffset_ = 0;
            fileRemaining_ = st.st_size;
            response_.addHeader("Server", "ccbWebServer/1.0");
            response_.addHeader("Cache-Control", "public, max-age=3600");
            response_.appendHeadersToBuffer(&writeBuf_, st.st_size);
        }
        else
        {
            LOG_ERROR("Failed to open file fd=%d path=%s", fd_, filePath.c_str());
            response_.setStatusCode(500);
            response_.setStatusMessage("Internal Server Error");
            response_.setContentType("text/html");
            response_.setBody("<html><body><h1>500 Internal Server Error</h1></body></html>");
            response_.setKeepAlive(false);
            response_.addHeader("Server", "ccbWebServer/1.0");
            response_.appendToBuffer(&writeBuf_);
        }
    }
    else
    {
        // 文件不存在
        LOG_WARN("File not found fd=%d path=%s", fd_, filePath.c_str());
        response_.setStatusCode(404);
        response_.setStatusMessage("Not Found");
        response_.setContentType("text/html");
        response_.setBody("<html><body><h1>404 Not Found</h1></body></html>");
        response_.setKeepAlive(false);
        response_.addHeader("Server", "ccbWebServer/1.0");
        response_.appendToBuffer(&writeBuf_);
    }

    LOG_INFO("Response %d %s fd=%d path=%s", 
        response_.statusCode(), response_.statusMessage().c_str(), fd_, filePath.c_str());
    
    return true;
}

}