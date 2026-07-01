#include "pool/sqlconnpool/Connection.h"

#include <vector>

namespace ccb
{
MysqlConnection::MysqlConnection()
{
    conn_ = mysql_init(nullptr);
}

MysqlConnection::~MysqlConnection()
{
    if(conn_ != nullptr)
    {
        mysql_close(conn_);
        
    }
}

bool MysqlConnection::connect(const std::string& ip, const std::string& user, const std::string& password,const unsigned int port,const std::string& dbname)
{
    MYSQL* p = mysql_real_connect(
                conn_, 
                ip.c_str(), 
                user.c_str(), 
                password.c_str(), 
                dbname.c_str(),
                port, 
                nullptr,
                0
            );

    return p != nullptr;
    
}

bool MysqlConnection::update(const std::string& sql) 
{
	if (mysql_query(conn_, sql.c_str())) 
    {
		return false;
	}
    
	return true;
}

//查询操作 select
MYSQL_RES* MysqlConnection::query(const std::string& sql) 
{
	if (mysql_query(conn_, sql.c_str())) 
    {
		return nullptr;
	}
	return mysql_use_result(conn_);
}

std::string MysqlConnection::escapeString(const std::string& str)
{
    if(str.empty() || conn_ == nullptr)
        return str;
    
    // mysql_real_escape_string 最坏情况每个字符都需要转义（×2），+1 为 '\0'
    size_t len = str.size() * 2 + 1;
    std::vector<char> buf(len);

    size_t outLen = mysql_real_escape_string(conn_, buf.data(), str.c_str(), str.size());

    return std::string(buf.data(), outLen);
}
    
}


