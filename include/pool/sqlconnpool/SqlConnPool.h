#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <atomic>
#include <memory>
#include <thread>
#include <functional>

#include "Connection.h"
#include "common/Noncopyable.h"

namespace ccb
{
class SqlConnectionPool : private Noncopyable
{

public:
    static SqlConnectionPool* getInstance();
    std::shared_ptr<MysqlConnection> getConnection();
    SqlConnectionPool() = default;
    ~SqlConnectionPool();
    void init(const std::string& ip,
            const std::string& user,
            const std::string& pwd,
            const unsigned int port,
            const std::string& dbname,
            const int minSize,
            const int maxSize,
            const int maxIdleTime,   
            const int connectionTimeout,
            bool isRunning = true);
    
    void start();

private:
    // 专门负责生成新连接
    void produceConnectionTask();
    // 定时扫描，释放多余的连接
    void scannerConnectionTask();


private:
    /* mysql的连接信息 */
    std::string ip_;
    std::string user_;
    std::string pwd_;
    unsigned int port_;
    std::string dbname_;

    /* 连接池的参数 */
    int minSize_;
    int maxSize_;
    int maxIdleTime_;    // 连接池最大空闲时间
    int connectionTimeout_;  //连接池获取连接的超时时间

    std::queue<MysqlConnection*> connQueue_;  // 存储mysql连接的队列
    std::mutex queueMutex_;

    std::atomic_int connectionCnt_;      // 记录连接所创建的connection连接的总数量
    std::condition_variable notEmpty_;
    std::atomic<bool> isRunning_{false};

    std::thread produceThread_, scannerThread_;

};    
}


