#pragma once

#include <string>
#include "common/Noncopyable.h"

namespace ccb
{

struct ServerConfig
{
    // ========== 通用类 ==========
    struct GeneralConfig
    {
        int port = 8080;
        int trigMode = 3;               // 0: LT+LT, 1: ET+LT, 2: LT+ET, 3: ET+ET
        std::string webRoot = "./webroot";
    } general;

    // ========== 日志类 ==========
    struct LogConfig
    {
        bool enableLog = true;
        std::string logDir = "logs";
        std::string logBaseName = "server";
        int logLevel = 0;               // 0:DEBUG, 1:INFO, 2:WARN, 3:ERROR, 4:FATAL
        off_t logRollSize = 100 * 1024 * 1024;
        int flushInterval = 3;          // 刷新间隔(秒)
    } log;

    // ========== 定时器类 ==========
    struct TimerConfig
    {
        int connectionTimeoutMs = 60000; // 连接最大空闲超时(ms)
    } timer;

    // ========== 线程池类 ==========
    struct ThreadPoolConfig
    {
        int minSize = 4;                 // 最小线程数
        int maxSize = 8;                 // 最大线程数
        int taskQueueMaxSize = 1024;     // 任务队列容量
        int mode = 0;                    // 0:FIXED, 1:CACHED
        bool startRunning = true;        // 启动时即开始运行
        int rejectPolicy = 3;            // 拒绝策略：0:THROW, 1:BLOCK, 2:CALLER_RUNS, 3:DISCARD, 4:DISCARD_OLDEST
        int idleTimeoutMs = 60000;       // 空闲线程超时(ms)
    } threadPool;

    // ========== 连接池类(MySQL) ==========
    struct ConnectionPoolConfig
    {
        bool enable = false;
        std::string host = "127.0.0.1";
        int port = 3306;
        std::string user = "root";
        std::string pwd = "123456";
        std::string dbName = "webserver";
        int minSize = 4;
        int maxSize = 32;
        int maxIdleTime = 300;           // 最大空闲时间(秒)
        int connectionTimeout = 1000;    // 获取连接超时(ms)
    } connPool;
};

class Config : private Noncopyable
{
public:
    static Config& instance();

    bool load(const std::string& path);

    const ServerConfig& serverConfig() const { return config_; }

private:
    ServerConfig config_;
};

} // namespace ccb
