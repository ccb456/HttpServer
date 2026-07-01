#pragma once

#include <string>
#include <unordered_map>

namespace ccb
{

class Buffer;

class HttpResponse
{

public:
    HttpResponse() { reset(); }
    ~HttpResponse() = default;

    void reset();

    void setStatusCode(int code)                    { statusCode_ = code; }
    int statusCode() const                          { return statusCode_; }
    void setStatusMessage(const std::string& msg)   { statusMessage_ = msg; }
    const std::string& statusMessage() const        { return statusMessage_; }
    void setContentType(const std::string& type)    { contentType_ = type; }
    void setBody(const std::string& body)           { body_ = body; }
    void addHeader(const std::string& key, const std::string& value);
    void setKeepAlive(bool on)                     { keepAlive_ = on; } 

    /// 将完整 HTTP 响应（状态行 + 头部 + 空行 + 包体）写入 Buffer
    void appendToBuffer(Buffer* buf) const;

    /// 仅写 HTTP 头部（状态行 + 头部 + 空行），不含包体（用于 sendfile）
    void appendHeadersToBuffer(Buffer* buf, size_t bodySize) const;

    /// 根据文件后缀返回 Content-Type
    static std::string getContentType(const std::string& suffix);


private:
    int statusCode_;
    std::string statusMessage_;
    std::string contentType_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
    bool keepAlive_;
};


}