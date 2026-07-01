#include "server/Server.h"
#include "log/Logger.h"
#include "pool/sqlconnpool/SqlConnPool.h"

#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/epoll.h>
#include <csignal>

namespace ccb
{

Server* Server::instance_ = nullptr;

void Server::handleSignal(int sig)
{
    if (instance_)
    {
        LOG_INFO("Received signal %d, stopping server...", sig);
        instance_->stop_ = true;
    }
}

Server::Server(const ServerConfig& cfg)
    : config_(cfg),
      epoller_(1024),
      acceptor_(InetAddress(cfg.general.port)),
      threadPool_(
          static_cast<size_t>(cfg.threadPool.minSize),
          static_cast<size_t>(cfg.threadPool.maxSize),
          static_cast<size_t>(cfg.threadPool.taskQueueMaxSize),
          static_cast<ThreadPoolMode>(cfg.threadPool.mode),
          cfg.threadPool.startRunning,
          static_cast<RejectPolicy>(cfg.threadPool.rejectPolicy),
          std::chrono::milliseconds(cfg.threadPool.idleTimeoutMs))
{
    instance_ = this;
    ::signal(SIGINT, handleSignal);
    ::signal(SIGTERM, handleSignal);

    initEventMode(cfg.general.trigMode);
    acceptor_.listen();

    // 初始化 SQL 连接池
    if (cfg.connPool.enable)
    {
        SqlConnectionPool::getInstance()->init(
            cfg.connPool.host,
            cfg.connPool.user,
            cfg.connPool.pwd,
            cfg.connPool.port,
            cfg.connPool.dbName,
            cfg.connPool.minSize,
            cfg.connPool.maxSize,
            cfg.connPool.maxIdleTime,
            cfg.connPool.connectionTimeout
        );
        SqlConnectionPool::getInstance()->start();
        LOG_INFO("SQL connection pool started: %s:%d/%s (min=%d, max=%d)",
            cfg.connPool.host.c_str(), cfg.connPool.port,
            cfg.connPool.dbName.c_str(), cfg.connPool.minSize, cfg.connPool.maxSize);
    }

    // 设置静态文件根目录
    HttpConn::setSrcDir(cfg.general.webRoot);

    LOG_INFO("Server initialized on port %d, trigMode=%d", cfg.general.port, cfg.general.trigMode);
}

Server::~Server()
{
    stop();
    LOG_INFO("Server stopped successfully");
}

void Server::stop()
{
    stop_ = true;

    // 清理所有连接
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [fd, conn] : connections_)
    {
        epoller_.delFd(fd);
    }
    connections_.clear();
    timer_.clear();

    LOG_INFO("All connections cleaned up");
}

void Server::initEventMode(int trigMode)
{
    uint32_t listenEvent = EPOLLIN;
    uint32_t connEvent = EPOLLIN | EPOLLRDHUP;

    switch (trigMode)
    {
    case 1: // ET + LT
        listenEvent |= EPOLLET;
        break;
    case 2: // LT + ET
        connEvent |= EPOLLET;
        break;
    case 3: // ET + ET
        listenEvent |= EPOLLET;
        connEvent |= EPOLLET;
        break;
    default:
        break;
    }

    // 注册 listen fd
    epoller_.addFd(acceptor_.fd(), listenEvent | EPOLLIN);
}

void Server::run()
{
    LOG_INFO("Server starting event loop...");
    while (!stop_)
    {
        int timeoutMs = timer_.getNextTick();
        int n = epoller_.wait(timeoutMs);

        if (n < 0)
        {
            if (errno == EINTR)
            {
                // 被信号中断，继续循环（stop_ 已由信号处理器置为 true 时会退出）
                continue;
            }
            LOG_ERROR("epoll_wait error, errno=%d", errno);
            break;
        }

        for (int i = 0; i < n; ++i)
        {
            int fd = epoller_.getEventFd(i);
            uint32_t events = epoller_.getEvents(i);

            if (fd == acceptor_.fd())
            {
                handleListen();
            }
            else if (events & EPOLLIN)
            {
                handleRead(fd);
            }
            else if (events & EPOLLOUT)
            {
                handleWrite(fd);
            }
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                closeConnection(fd);
            }
            else
            {
                LOG_WARN("Unexpected event on fd=%d: 0x%x", fd, events);
            }
        }

        // 定时清理过期连接
        timer_.tick();
    }
    LOG_INFO("Server event loop exited");
}

void Server::handleListen()
{
    // ET 模式必须循环 accept，直到 EAGAIN，防止丢失并发连接
    while (true)
    {
        InetAddress peerAddr;
        int connFd = acceptor_.accept(&peerAddr);
        if (connFd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break; // 暂无更多连接
            }
            if (errno == EINTR)
            {
                continue; // 被信号中断，重试
            }
            LOG_ERROR("accept error, errno=%d", errno);
            break;
        }

        addConnection(connFd, peerAddr);
        LOG_INFO("New connection fd=%d from %s", connFd, peerAddr.toIpPort().c_str());
    }
}

void Server::addConnection(int fd, const InetAddress& addr)
{
    std::lock_guard<std::mutex> lock(mutex_);

    epoller_.addFd(fd, EPOLLIN | EPOLLRDHUP | EPOLLET);

    auto [it, ok] = connections_.try_emplace(fd, fd);
    it->second.setPeerAddr(addr);

    timer_.add(fd, config_.timer.connectionTimeoutMs, [this, fd]() {
        closeConnection(fd);
    });
}

void Server::closeConnectionLocked(int fd)
{
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    epoller_.delFd(fd);
    connections_.erase(fd);
    timer_.del(fd);
    LOG_INFO("Connection closed fd=%d", fd);
}

void Server::closeConnection(int fd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    closeConnectionLocked(fd);
}

void Server::handleRead(int fd)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    HttpConn& conn = it->second;
    timer_.adjust(fd, config_.timer.connectionTimeoutMs);

    ssize_t n = conn.read();
    LOG_DEBUG("Read %zd bytes from fd=%d", n, fd);
    if (n <= 0)
    {
        closeConnectionLocked(fd);
        return;
    }

    // 交给线程池处理
    threadPool_.submitTask([this, fd]() {
        onRequest(fd);
    });
}

void Server::handleWrite(int fd)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    HttpConn& conn = it->second;
    ssize_t n = conn.write();
    LOG_DEBUG("Wrote %zd bytes to fd=%d", n, fd);

    if (n < 0)
    {
        closeConnectionLocked(fd);
        return;
    }

    if (conn.hasMoreToWrite())
    {
        // 还有数据没写完,继续监听写事件
        epoller_.modFd(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET);
    }
    else
    {
        // 写完了
        if (conn.isKeepAlive())
        {
            // keep-alive 连接,继续等待请求
            epoller_.modFd(fd, EPOLLIN | EPOLLRDHUP | EPOLLET);
            timer_.adjust(fd, config_.timer.connectionTimeoutMs);
        }
        else
        {
            closeConnectionLocked(fd);
        }
    }
}

void Server::onRequest(int fd)
{
    // 仅短暂加锁检查连接是否存在
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connections_.find(fd) == connections_.end()) return;
    }

    // === 无锁处理：不阻塞其他连接 ===
    // 拿到 HttpConn 指针（map 不会 rehash，指针稳定且不会被其他线程删除，因为只有超时/hup 才删，worker 线程内安全）
    HttpConn* connPtr = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) return;
        connPtr = &it->second;
    }

    if (!connPtr->process())
    {
        LOG_DEBUG("onRequest fd=%d: incomplete request, waiting for more data", fd);
        return;
    }

    // 尝试直接写响应（省一次 epoll 往返）
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connections_.find(fd) == connections_.end()) return;

        // 先尝试直接 write
        ssize_t n = connPtr->write();
        if (n < 0)
        {
            closeConnectionLocked(fd);
            return;
        }

        if (connPtr->hasMoreToWrite())
        {
            // 没写完，注册 EPOLLOUT 继续写
            LOG_DEBUG("onRequest fd=%d: partial write, registering EPOLLOUT", fd);
            epoller_.modFd(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET);
        }
        else
        {
            // 写完了
            if (connPtr->isKeepAlive())
            {
                LOG_DEBUG("onRequest fd=%d: response sent, keep-alive", fd);
                epoller_.modFd(fd, EPOLLIN | EPOLLRDHUP | EPOLLET);
                timer_.adjust(fd, config_.timer.connectionTimeoutMs);
            }
            else
            {
                closeConnectionLocked(fd);
            }
        }
    }
}

} // namespace ccb
