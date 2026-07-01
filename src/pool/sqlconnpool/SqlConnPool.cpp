#include "pool/sqlconnpool/SqlConnPool.h"
#include "log/Logger.h"

namespace ccb
{

SqlConnectionPool::~SqlConnectionPool() 
{

    isRunning_ = false;
    // 唤醒等待的生产者线程和扫描线程
    notEmpty_.notify_all();

    if (produceThread_.joinable()) produceThread_.join(); // 等待生产者线程结束
    if (scannerThread_.joinable()) scannerThread_.join(); // 等待扫描线程结束

    // 释放连接池中的连接
    std::unique_lock<std::mutex> lock(queueMutex_);
    while (!connQueue_.empty()) {
        MysqlConnection* conn = connQueue_.front();
        connQueue_.pop();
        delete conn;
    }
    connectionCnt_ = 0;
}

// 线程安全的懒汉单例模式
SqlConnectionPool* SqlConnectionPool::getInstance()
{
    //对于静态对象的初始化会由编译器自动进行lock和unlock
    static SqlConnectionPool pool;
    return &pool;
}

void SqlConnectionPool::init(const std::string& ip,
    const std::string& user,
    const std::string& pwd,
    const unsigned int port,
    const std::string& dbname,
    const int minSize,
    const int maxSize,
    const int maxIdleTime,   // 连接池最大空闲时间
    const int connectionTimeout,
    bool isRunning)
{
    ip_ = ip;
    user_ = user;
    pwd_ = pwd;
    port_ = port;
    dbname_ = dbname;
    minSize_ = minSize;
    maxSize_ = maxSize;
    maxIdleTime_ = maxIdleTime;
    connectionTimeout_ = connectionTimeout;
    isRunning_ = isRunning;
}

void SqlConnectionPool::start()
{
    if(isRunning_)
    {
        // 创建初始数量的连接
        for(int i = 0; i < minSize_; ++i)
        {
            MysqlConnection* conn = new MysqlConnection;
            if(conn->connect(ip_, user_, pwd_, port_, dbname_))
            {
                conn->refreshAliveTime(); //刷新一下开始空闲的起始时间
                connQueue_.push(conn);
                ++connectionCnt_;
                LOG_INFO("SQL pool: created initial connection %d/%d", i + 1, minSize_);
            }
            else
            {
                LOG_ERROR("SQL pool: failed to create connection %d/%d (host=%s:%d, db=%s, user=%s)",
                    i + 1, minSize_, ip_.c_str(), port_, dbname_.c_str(), user_.c_str());
                return;
            }

        }

        // 启动一个工作线程,作为连接的生产者
        produceThread_ = std::thread(std::bind(&SqlConnectionPool::produceConnectionTask, this));
        
        //启动一个新的定时线程，扫描超过maxIdleTime时间的空闲连接，进行多余的连接回收
        scannerThread_ = std::thread(std::bind(&SqlConnectionPool::scannerConnectionTask, this));

        LOG_INFO("SQL pool: %d connections ready, producer & scanner started", connectionCnt_.load());
    }

}
void SqlConnectionPool::produceConnectionTask()
{
    while(isRunning_)
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        while(!connQueue_.empty() && isRunning_)
        {
            notEmpty_.wait(lock);    // 队列不空，生产者线程进入等待状态
        }

        if (!isRunning_) break;

        // 队列空了，创建新的连接
        if(connectionCnt_ < maxSize_)
        {
            MysqlConnection* p = new MysqlConnection;

            if(p->connect(ip_, user_, pwd_, port_, dbname_))
            {
                p->refreshAliveTime(); //刷新一下开始空闲的起始时间  
                connQueue_.push(p);
                ++connectionCnt_;
                notEmpty_.notify_all();
            }
            else
            {
                
                return;
            }
        }

    }
}

std::shared_ptr<MysqlConnection> SqlConnectionPool::getConnection()
{
    std::unique_lock<std::mutex> lock(queueMutex_);

    while(connQueue_.empty())
    {
        if(std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::milliseconds(connectionTimeout_)))
        {
            if(connQueue_.empty())
            {
                return nullptr;
            }
        }

    }

    std::shared_ptr<MysqlConnection> sp(connQueue_.front(),[this](MysqlConnection* conn) {
                                    std::unique_lock<std::mutex> lock(queueMutex_);
                                    if (isRunning_) { // 仅在连接池运行时放回队列
                                        connQueue_.push(conn);
                                    } else {
                                        delete conn; // 析构时直接释放连接
                                    }
                                });
    connQueue_.pop();

    if(connQueue_.empty())
    {
        notEmpty_.notify_all();  // 谁消费了最后一个连接，谁去唤醒生产者线程
    }

    return sp;
}

void SqlConnectionPool::scannerConnectionTask()
{
    while(isRunning_)
    {
        std::this_thread::sleep_for(std::chrono::seconds(maxIdleTime_));

        std::unique_lock<std::mutex> locker(queueMutex_);
        while(connectionCnt_ > minSize_)
        {
            MysqlConnection* p = connQueue_.front();
            if(p->getAliveTime() >= (maxIdleTime_ * 1000))
            {
                connQueue_.pop();
                --connectionCnt_;
                delete p;
            }
            else
            {
                break;  // 队头没有超过时间，其它的也就没有
            }
        }
    }
}
}