#include "common/Config.h"
#include "log/Logger.h"

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>

namespace ccb
{

Config& Config::instance()
{
    static Config cfg;
    return cfg;
}

static std::string trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool Config::load(const std::string& path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open())
    {
        LOG_WARN("Config file not found: %s, using default config", path.c_str());
        return false;
    }

    LOG_INFO("Loading config from %s", path.c_str());

    std::string line;
    while (std::getline(ifs, line))
    {
        // 去除首尾空白
        line = trim(line);

        // 空行或注释
        if (line.empty() || line[0] == '#') continue;

        // 支持 '=' 或 ':' 分隔
        size_t sep = line.find('=');
        if (sep == std::string::npos) sep = line.find(':');
        if (sep == std::string::npos) continue;

        std::string key = trim(line.substr(0, sep));
        std::string val = trim(line.substr(sep + 1));

        if (key.empty()) continue;

        // ——— 通用类 ———
        if (key == "port")                          config_.general.port = std::stoi(val);
        else if (key == "trigMode")                 config_.general.trigMode = std::stoi(val);
        else if (key == "webRoot")                  config_.general.webRoot = val;
        // ——— 日志类 ———
        else if (key == "enableLog")                config_.log.enableLog = (val == "true" || val == "1");
        else if (key == "logDir")                   config_.log.logDir = val;
        else if (key == "logBaseName")              config_.log.logBaseName = val;
        else if (key == "logLevel")                 config_.log.logLevel = std::stoi(val);
        else if (key == "logRollSize")              config_.log.logRollSize = std::stoll(val);
        else if (key == "logFlushInterval")         config_.log.flushInterval = std::stoi(val);
        // ——— 定时器类 ———
        else if (key == "timeoutMs")                config_.timer.connectionTimeoutMs = std::stoi(val);
        // ——— 线程池类 ———
        else if (key == "threadMinSize")            config_.threadPool.minSize = std::stoi(val);
        else if (key == "threadMaxSize")            config_.threadPool.maxSize = std::stoi(val);
        else if (key == "taskQueueMaxSize")         config_.threadPool.taskQueueMaxSize = std::stoi(val);
        else if (key == "threadPoolMode")           config_.threadPool.mode = std::stoi(val);
        else if (key == "threadStartRunning")       config_.threadPool.startRunning = (val == "true" || val == "1");
        else if (key == "threadRejectPolicy")       config_.threadPool.rejectPolicy = std::stoi(val);
        else if (key == "threadIdleTimeoutMs")      config_.threadPool.idleTimeoutMs = std::stoi(val);
        // ——— 连接池类(MySQL) ———
        else if (key == "enableSqlPool")            config_.connPool.enable = (val == "true" || val == "1");
        else if (key == "sqlHost")                  config_.connPool.host = val;
        else if (key == "sqlPort")                  config_.connPool.port = std::stoi(val);
        else if (key == "sqlUser")                  config_.connPool.user = val;
        else if (key == "sqlPwd")                   config_.connPool.pwd = val;
        else if (key == "sqlDbName")                config_.connPool.dbName = val;
        else if (key == "sqlMinSize")               config_.connPool.minSize = std::stoi(val);
        else if (key == "sqlMaxSize")               config_.connPool.maxSize = std::stoi(val);
        else if (key == "sqlMaxIdleTime")           config_.connPool.maxIdleTime = std::stoi(val);
        else if (key == "sqlConnectionTimeout")     config_.connPool.connectionTimeout = std::stoi(val);
    }

    LOG_INFO("Config loaded: port=%d, webRoot=%s, logDir=%s, logLevel=%d, threadMin=%d, threadMax=%d",
        config_.general.port, config_.general.webRoot.c_str(), config_.log.logDir.c_str(), config_.log.logLevel,
        config_.threadPool.minSize, config_.threadPool.maxSize);

    return true;
}

} // namespace ccb
