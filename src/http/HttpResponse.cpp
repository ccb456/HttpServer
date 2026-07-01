#include "http/HttpResponse.h"
#include "net/Buffer.h"

#include <cstdio>

namespace ccb
{

static const std::unordered_map<std::string, std::string> kSuffixToType = 
{
    {".html", "text/html"},
    {".htm",  "text/html"},
    {".css",  "text/css"},
    {".js",   "application/javascript"},
    {".json", "application/json"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".ico",  "image/x-icon"},
    {".svg",  "image/svg+xml"},
    {".mp4",  "video/mp4"},
    {".mp3",  "audio/mpeg"},
    {".pdf",  "application/pdf"},
    {".txt",  "text/plain"},
    {".xml",  "application/xml"},
};

void HttpResponse::reset()
{
    statusCode_ = 200;
    statusMessage_ = "OK";
    contentType_ = "text/html";
    body_.clear();
    headers_.clear();
    keepAlive_ = false;
}

void HttpResponse::addHeader(const std::string& key, const std::string& value)
{
    headers_[key] = value;
}

void HttpResponse::appendToBuffer(Buffer* buf) const
{
    appendHeadersToBuffer(buf, body_.size());

    // 包体
    if(!body_.empty())
    {
        buf->append(body_);
    }
}

void HttpResponse::appendHeadersToBuffer(Buffer* buf, size_t bodySize) const
{
    char headerBuf[256];

    // 状态行: HTTP/1.1 200 OK\r\n
    int n = snprintf(headerBuf, sizeof(headerBuf), "HTTP/1.1 %d %s\r\n", statusCode_, statusMessage_.c_str());
    buf->append(headerBuf, n);

    // Content-Type
    buf->append("Content-Type: ");
    buf->append(contentType_);
    buf->append("\r\n");

    // Content-Length
    n = snprintf(headerBuf, sizeof(headerBuf), "Content-Length: %zu\r\n", bodySize);
    buf->append(headerBuf, n);

    // Connection
    buf->append(keepAlive_ ? "Connection: keep-alive\r\n" : "Connection: close\r\n");

    // 自定义头部
    for(const auto& [key, value] : headers_)
    {
        buf->append(key);
        buf->append(": ");
        buf->append(value);
        buf->append("\r\n");
    }

    // 空行分隔 Header 和 Body
    buf->append("\r\n");
}

std::string HttpResponse::getContentType(const std::string& suffix)
{
    auto it = kSuffixToType.find(suffix);
    return it != kSuffixToType.end() ? it->second : "application/octet-stream";
}

}
