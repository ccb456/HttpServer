#pragma once

#include "http/HttpRequest.h"

namespace ccb
{

class Buffer;

class HttpParser
{
public:
    enum class ParseState
    {
        kParseRequestLine,
        kParseHeaders,
        kParseBody,
        kParseComplete,
        kParseError

    };

    HttpParser() : state_(ParseState::kParseRequestLine), contentLength_(0) {}

    ParseState parse(Buffer* buf);

    HttpRequest& request() { return request_; }
    const HttpRequest& request()const { return request_; }
    
    void reset();
    bool isComplete() const { return state_ == ParseState::kParseComplete; }
    bool hasError() const { return state_ == ParseState::kParseError; }

private:
    bool processRequestLine(const std::string& line);
    bool processHeader(const std::string& line);


private:
    ParseState state_;
    HttpRequest request_;
    size_t contentLength_;
};
}