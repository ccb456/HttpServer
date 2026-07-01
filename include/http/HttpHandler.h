#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include <string>

namespace ccb
{

class HttpHandler
{
public:
    // 处理post / Login 验证用户名密码，设置response
    static void handleLogin(const HttpRequest& req, HttpResponse& resp);

    // 处理post /register，插入新用户，设置response
    static void handleRegister(const HttpRequest& req, HttpResponse& resp);

private:
    // URL 解码(%xx -> 字符)
    static std::string urlDecode(const std::string& src);

    // 解析 application/x-www-form-urlencoded 的 body 为 key-value
    static void parseFormBody(const std::string& body, std::string& username, std::string& pwd);

    // 读取静态文件
    static std::string readFile(const std::string& filepath);
};

} // namespace ccb
