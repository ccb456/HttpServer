#include "http/HttpHandler.h"
#include "pool/sqlconnpool/SqlConnPool.h"
#include "http/HttpConn.h"
#include "log/Logger.h"

#include <fstream>
#include <ostream>
#include <cstring>
#include <iomanip>

namespace ccb
{

void HttpHandler::handleLogin(const HttpRequest& req, HttpResponse& resp)
{
    // 1. 解析表单参数
    std::string username, password;
    parseFormBody(req.body(), username, password);
    LOG_INFO("Login attempt: username=%s", username.c_str());

    if(username.empty() || password.empty())
    {
        // 参数不完整 -> 返回登录错误页面
        std::string html = readFile(HttpConn::srcDir() + "/logError.html");
        resp.setStatusCode(200);
        resp.setStatusMessage("OK");
        resp.setContentType("text/html");
        resp.setBody(html.empty() 
                    ? "<html><body><h1>用户名或密码不能为空</h1></body></html>"
                    : html);
        resp.setKeepAlive(false);

        return;
    }

    // 2. 获取数据库连接
    auto conn = SqlConnectionPool::getInstance()->getConnection();
    if(!conn)
    {
        LOG_ERROR("Login: failed to get DB connection");
        resp.setStatusCode(500);
        resp.setStatusMessage("Internal Server Error");
        resp.setContentType("text/html");
        resp.setBody("<html><body><h1>500 服务器内部错误</h1></body></html>");
        resp.setKeepAlive(false);
        return;
    }

    // 3. 转义输入，防止SQL注入
    auto escapedUser = conn->escapeString(username);
    auto escapedPwd = conn->escapeString(password);

    // 4. 查询，用mysql内置SHA2 比较密码
    std::string sql = "SELECT id FROM users WHERE username = '" + escapedUser
                        + "' AND password = SHA2('" + escapedPwd + "', 256)";
    MYSQL_RES* res = conn->query(sql);
    if (!res)
    {
        LOG_ERROR("Login: SQL query failed for username=%s", username.c_str());
        resp.setStatusCode(500);
        resp.setStatusMessage("Internal Server Error");
        resp.setContentType("text/html");
        resp.setBody("<html><body><h1>500 服务器内部错误</h1></body></html>");
        resp.setKeepAlive(false);
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    bool success = (row != nullptr);
    mysql_free_result(res);

    // 根据结果返回不同页面
    if (success)
    {
        LOG_INFO("Login success: username=%s", username.c_str());
        std::string html = readFile(HttpConn::srcDir() + "/welcome.html");
        resp.setStatusCode(200);
        resp.setStatusMessage("OK");
        resp.setContentType("text/html");
        resp.setBody(html.empty()
            ? "<html><body><h1>Welcome!</h1></body></html>"
            : html);
    }
    else
    {
        LOG_INFO("Login failed: username=%s", username.c_str());
        std::string html = readFile(HttpConn::srcDir() + "/logError.html");
        resp.setStatusCode(200);
        resp.setStatusMessage("OK");
        resp.setContentType("text/html");
        resp.setBody(html.empty()
            ? "<html><body><h1>用户名或密码错误</h1></body></html>"
            : html);
    }

    resp.setKeepAlive(false);
}

void HttpHandler::handleRegister(const HttpRequest& req, HttpResponse& resp)
{
    // 1. 解析表单参数
    std::string username, password;
    parseFormBody(req.body(), username, password);

    LOG_INFO("Register attempt: username=%s", username.c_str());

    if (username.empty() || password.empty())
    {
        std::string html = readFile(HttpConn::srcDir() + "/registerError.html");
        resp.setStatusCode(200);
        resp.setStatusMessage("OK");
        resp.setContentType("text/html");
        resp.setBody(html.empty()
            ? "<html><body><h1>用户名或密码不能为空</h1></body></html>"
            : html);
        resp.setKeepAlive(false);
        return;
    }

    // 2. 获取数据库连接
    auto conn = SqlConnectionPool::getInstance()->getConnection();
    if (!conn)
    {
        LOG_ERROR("Register: failed to get DB connection");
        resp.setStatusCode(500);
        resp.setStatusMessage("Internal Server Error");
        resp.setContentType("text/html");
        resp.setBody("<html><body><h1>500 服务器内部错误</h1></body></html>");
        resp.setKeepAlive(false);
        return;
    }

    // 3. 转义输入
    auto escapedUser = conn->escapeString(username);
    auto escapedPwd  = conn->escapeString(password);

    // 4. 先检查用户名是否已存在
    std::string checkSql =
        "SELECT id FROM users WHERE username = '" + escapedUser + "'";
    MYSQL_RES* checkRes = conn->query(checkSql);
    if (!checkRes)
    {
        LOG_ERROR("Register: check SQL failed for username=%s", username.c_str());
        resp.setStatusCode(500);
        resp.setStatusMessage("Internal Server Error");
        resp.setContentType("text/html");
        resp.setBody("<html><body><h1>500 服务器内部错误</h1></body></html>");
        resp.setKeepAlive(false);
        return;
    }

    bool exists = (mysql_fetch_row(checkRes) != nullptr);
    mysql_free_result(checkRes);

    if (exists)
    {
        // 用户名已存在
        LOG_INFO("Register failed: username=%s already exists", username.c_str());
        std::string html = readFile(HttpConn::srcDir() + "/registerError.html");
        resp.setStatusCode(200);
        resp.setStatusMessage("OK");
        resp.setContentType("text/html");
        resp.setBody(html.empty()
            ? "<html><body><h1>用户名已存在</h1></body></html>"
            : html);
        resp.setKeepAlive(false);
        return;
    }

    // 5. 插入新用户（用 MySQL 内置 SHA2 哈希密码）
    std::string insertSql =
        "INSERT INTO users (username, password) VALUES ('" + escapedUser
        + "', SHA2('" + escapedPwd + "', 256))";

    if (conn->update(insertSql))
    {
        // 注册成功 → 302 重定向到登录页
        LOG_INFO("Register success: username=%s", username.c_str());
        resp.setStatusCode(302);
        resp.setStatusMessage("Found");
        resp.addHeader("Location", "/login");
        resp.setContentType("text/html");
        resp.setBody("<html><body>注册成功, <a href='/login'>去登录</a></body></html>");
    }
    else
    {
        LOG_ERROR("Register: insert failed for username=%s", username.c_str());
        std::string html = readFile(HttpConn::srcDir() + "/registerError.html");
        resp.setStatusCode(200);
        resp.setStatusMessage("OK");
        resp.setContentType("text/html");
        resp.setBody(html.empty()
            ? "<html><body><h1>注册失败，请重试</h1></body></html>"
            : html);
    }

    resp.setKeepAlive(false);

}

std::string HttpHandler::urlDecode(const std::string& src)
{
    std::string result;
    result.reserve(src.size());

    for(size_t i = 0; i < src.size(); ++i)
    {
        if(src[i] == '%' && i + 2 < src.size())
        {
            int high = 0, low = 0;
            char h = src[i + 1], lo = src[i + 2];
            if(h >= '0' && h <= '9') high = h - '0';
            else if (h >= 'A' && h <= 'F')
            {
                high = h - 'A' + 10;
            }
            else if (h >= 'a' && h <= 'f')
            {
                high = h - 'a' + 10;
            }

            if(lo >= '0' && lo <= '9') low = lo - '0';
            else if (lo >= 'A' && lo <= 'F')
            {
                low = lo - 'A' + 10;
            }
            else if (lo >= 'a' && lo <= 'f')
            {
                low = lo - 'a' + 10;
            }

            result += static_cast<char>((high << 4) | low);
            i += 2; 
        }
        else if(src[i] == '+')
        {
            result += ' ';
        }
        else
        {
            result += src[i];
        }
    }

    return result;
}

void HttpHandler::parseFormBody(const std::string& body, 
                                    std::string& username, 
                                    std::string& pwd)
{
    username.clear();
    pwd.clear();

    size_t pos = 0;
    while(pos < body.size())
    {
        size_t eq = body.find('=', pos);
        size_t amp = body.find('&', pos);

        if(eq == std::string::npos || eq >= amp)
        {
            // 跳过格式不对的字段
            pos = (amp == std::string::npos) ? body.size() : amp + 1;
            continue;
        }

        std::string key = urlDecode(body.substr(pos, eq - pos));
        std::string value;

        if(amp == std::string::npos)
        {
            value = urlDecode(body.substr(eq + 1));
            pos = body.size();
        }
        else
        {
            value = urlDecode(body.substr(eq + 1, amp - eq - 1));
            pos = amp + 1;
        }

        if(key == "username") username = value;
        else if(key == "password") pwd = value;
    }
}
std::string HttpHandler::readFile(const std::string& filepath)
{
    std::ifstream ifs(filepath, std::ios::binary);
    if(!ifs) return "";
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

} // namespace ccb
