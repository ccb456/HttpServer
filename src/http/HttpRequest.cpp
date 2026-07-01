#include "http/HttpRequest.h"

#include <algorithm>
#include <cctype>

namespace ccb
{

void HttpRequest::reset()
{
    method_ = Method::kUnknown;
    version_ = Version::kUnknownVersion;

    path_.clear();
    query_.clear();
    body_.clear();
    headers_.clear();
}

bool HttpRequest::hasHeader(const std::string& key) const
{
    return headers_.find(key) != headers_.end();
}

std::string HttpRequest::getHeader(const std::string& key) const
{
    // 大小写不敏感查找
    std::string keyLower = key;
    for(auto& c : keyLower) c = static_cast<char>(::tolower(c));

    for(const auto& [k, v] : headers_)
    {
        std::string kLower = k;
        for(auto& c : kLower) c = static_cast<char>(::tolower(c));
        if(kLower == keyLower) return v;
    }
    return "";
}

void HttpRequest::addHeader(const std::string& key, const std::string& value)
{
    headers_[key] = value;
}

bool HttpRequest::isKeepAlive() const
{
    if(version_ == Version::kV1_1)
    {
        auto val = getHeader("Connection");
        return val.empty() || val != "close";
    }

    if(version_ == Version::kV1_0)
    {
        auto val = getHeader("Connection");
        return !val.empty() && val == "keep-alive";
    }

    return false;
}

std::string HttpRequest::methodString() const
{
    switch (method_)
    {
        case Method::kGet:      return "GET";
        case Method::kPost:     return "POST";
        case Method::kHead:     return "HEAD";
        case Method::kPut:      return "PUT";
        case Method::kDelete:   return "DELETE";    
        default:                return "UNKNOWN";
    }
}

}