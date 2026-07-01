#pragma once

/* 该文件主要实现 数据库操作代码、增删改查代码 */

#include <string>
#include <mysql/mysql.h>
#include <ctime>

namespace ccb
{
class MysqlConnection
{
public:
	//初始化数据库连接
	MysqlConnection();

	//释放数据库连接资源
	~MysqlConnection();

	//连接数据库
	bool connect(const std::string& ip, const std::string& user, const std::string& password,const unsigned int port,const std::string& dbname);

	//更新操作 insert、delete、update
	bool update(const std::string& sql);

	//查询操作 select
	MYSQL_RES* query(const std::string& sql);

	//刷新一下连接的起始的空闲时间点
	void refreshAliveTime()
	{
		alivetime_ = clock();
	}

	//返回存活的时间
	clock_t getAliveTime() const
	{
		return clock() - alivetime_;
	}

	// 对字符串进行 SQL 转义，防止注入
	std::string escapeString(const std::string& str);

private:
	MYSQL* conn_;               //表示和 MySQL Server 的一条连接
	clock_t alivetime_;         //记录进入空闲状态后的起始存活时间

};

}
