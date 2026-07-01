#pragma once

#include <string>
#include <unordered_map>

namespace ccb
{
class HttpRequest
{

public:
    enum class Method{kGet, kPost, kHead, kPut, kDelete, kUnknown};
    enum class Version{ kV1_0, kV1_1, kUnknownVersion};

    HttpRequest() { reset();}
    ~HttpRequest() = default;

    void reset();

    Method method() const { return method_;}
    Version version() const { return version_;}

    const std::string& path() const { return path_;}
    const std::string& query() const { return query_;}
    const std::string& body() const { return body_;}

    void setMethod(Method m)           { method_ = m;}
    void setVersion(Version v)         { version_ = v; }
    void setPath(const std::string& p) { path_ = p; }
    void setQuery(const std::string& q){ query_ = q; }
    void setBody(const std::string& b) { body_ = b; }

    bool hasHeader(const std::string& key) const;
    std::string getHeader(const std::string& key) const;
    void addHeader(const std::string& key, const std::string& value);

    bool isKeepAlive() const;
    std::string methodString() const;

private:
    Method method_;
    Version version_;
    std::string path_;
    std::string query_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;

};
}