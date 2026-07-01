#include "common/Config.h"
#include "server/Server.h"
#include "log/Logger.h"

#include <cstdlib>

int main(int argc, char* argv[])
{
    // 加载配置
    const char* configPath = "config/server.yaml";
    if (argc > 1)
        configPath = argv[1];

    bool cfgLoaded = ccb::Config::instance().load(configPath);
    if (!cfgLoaded)
    {
        // 配置文件不存在,使用默认配置
    }

    const auto& cfg = ccb::Config::instance().serverConfig();

    // 初始化日志
    if (cfg.log.enableLog)
    {
        ccb::Logger::init(cfg.log.logBaseName,
                          static_cast<ccb::Logger::LogLevel>(cfg.log.logLevel),
                          cfg.log.logRollSize,
                          cfg.log.flushInterval,
                          cfg.log.logDir);
        LOG_INFO("Logger initialized: dir=%s, level=%d, rollSize=%lld, flushInterval=%d",
            cfg.log.logDir.c_str(), cfg.log.logLevel, static_cast<long long>(cfg.log.logRollSize), cfg.log.flushInterval);
    }
    else
    {
        // 日志未启用，仅输出到 stderr 提示
        fprintf(stderr, "Warning: Log is disabled in config\n");
    }

    if (!cfgLoaded)
    {
        LOG_WARN("Running with default config (no config file found)");
    }

    LOG_INFO("HttpServer starting...");

    ccb::Server server(cfg);
    server.run();

    return 0;
}
