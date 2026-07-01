#include "http/HttpParser.h"
#include "net/Buffer.h"
#include "log/Logger.h"

#include <cstring>
#include <cstdio>
#include <cctype>

namespace ccb
{
HttpParser::ParseState HttpParser::parse(Buffer* buf)
{
    const char* data = buf->peek();
    size_t remaining = buf->readableBytes();

    while(remaining > 0 && state_ != ParseState::kParseComplete &&
        state_ != ParseState::kParseError)
    {
        if(state_ == ParseState::kParseRequestLine || state_ == ParseState::kParseHeaders)
        {
            // 查找\r\n
            const char* crlf = nullptr;
            for(size_t i = 0; i + 1 < remaining; ++i)
            {
                if(data[i] == '\r' && data[i + 1] == '\n')
                {
                    crlf = data + i;
                    break;
                }
            }

            if(!crlf) break;  //不完整行，等待更多数据

            std::string line(data, crlf - data);
            size_t lineLen = (crlf - data) + 2;     //含 \r\n

            if(state_ == ParseState::kParseRequestLine)
            {
                if(!processRequestLine(line))
                {
                    state_ = ParseState::kParseError;
                    break;
                }

                state_ = ParseState::kParseHeaders;
            }
            else
            {
                if(line.empty())
                {
                    // 空行 -> 头部结束
                    state_ = (contentLength_ > 0) ? ParseState::kParseBody : ParseState::kParseComplete;

                }
                else
                {
                    if(!processHeader(line))
                    {
                        state_ = ParseState::kParseError;
                        break;
                    }
                }
            }

            data += lineLen;
            remaining -= lineLen;
        }else if(state_ == ParseState::kParseBody)
        {
            if(remaining >= contentLength_)
            {
                request_.setBody(std::string(data, contentLength_));

                data += contentLength_;
                remaining -= contentLength_;
                state_ = ParseState::kParseComplete;
            }
            else
            {
                break; // 包体未到齐
            }
        }
    
    }

    // 通知buffer消费了多少字节
    buf->retrieve(buf->readableBytes() - remaining);

    return state_;
}
void HttpParser::reset()
{
    state_ = ParseState::kParseRequestLine;
    request_.reset();
    contentLength_ = 0;
}

bool HttpParser::processRequestLine(const std::string& line)
{
    // line = "GET /path?key=val HTTP/1.1"
    size_t methodEnd = line.find(' ');
    if(methodEnd == std::string::npos) return false;

    std::string methodStr = line.substr(0, methodEnd);
    if(methodStr == "GET") 
    {
        request_.setMethod(HttpRequest::Method::kGet);
    }
    else if(methodStr == "POST") 
    {
        request_.setMethod(HttpRequest::Method::kPost);
    }
    else if(methodStr == "HEAD")
    {

        request_.setMethod(HttpRequest::Method::kHead);
    } 
    else if(methodStr == "PUT") 
    {
        request_.setMethod(HttpRequest::Method::kPut);
    }
    else if(methodStr == "DELETE")
    {

        request_.setMethod(HttpRequest::Method::kDelete);
    } 
    else
    {
        LOG_WARN("Unknown HTTP method: %s", methodStr.c_str());
        request_.setMethod(HttpRequest::Method::kUnknown);
    }
    
    // 解析uri
    size_t uriStart = methodEnd + 1;
    size_t verStart = line.find(' ', uriStart);
    if(verStart == std::string::npos) return false;

    std::string uri = line.substr(uriStart, verStart - uriStart);
    
    // 分离path 和 query
    size_t queryPos = uri.find('?');
    if(queryPos != std::string::npos)
    {
        request_.setPath(uri.substr(0, queryPos));
        request_.setQuery(uri.substr(queryPos + 1));
    }
    else
    {
        request_.setPath(uri);
    }

    // 解析版本

    std::string ver = line.substr(verStart + 1);
    if(ver == "HTTP/1.1")   
    {
        request_.setVersion(HttpRequest::Version::kV1_1);
    }
    else if(ver == "HTTP/1.0")
    {
        request_.setVersion(HttpRequest::Version::kV1_0);
    }
    else
    {
        LOG_WARN("Unknown HTTP version: %s", ver.c_str());
        request_.setVersion(HttpRequest::Version::kUnknownVersion);
    }

    return true;

}

bool HttpParser::processHeader(const std::string& line)
{
    // line = "Key: Value"
    size_t colon = line.find(':');
    if(colon == std::string::npos) return false;
    
    std::string key = line.substr(0, colon);

    // Value 跳过 ": " 两个字符
    std::string value = (colon + 2 < line.size()) ? line.substr(colon + 2) : "";

    request_.addHeader(key, value);

    // 兼容大小写的 header 名
    std::string keyLower = key;
    for(auto& c : keyLower) c = static_cast<char>(::tolower(c));

    if(keyLower == "content-length")
    {
        contentLength_ = static_cast<size_t>(std::stoi(value.c_str()));
    }

    return true;
}

}