#pragma once

#include "common/Noncopyable.h"
#include "common/Config.h"
#include "net/Epoller.h"
#include "net/Acceptor.h"
#include "net/InetAddress.h"
#include "http/HttpConn.h"
#include "timer/HeapTimer.h"
#include "pool/threadpool/ThreadPool.h"

#include <unordered_map>
#include <mutex>
#include <atomic>

namespace ccb
{

class Server : private Noncopyable
{
public:
    explicit Server(const ServerConfig& cfg);
    ~Server();

    void run();

private:
    void initEventMode(int trigMode);
    void addConnection(int fd, const InetAddress& addr);
    void closeConnection(int fd);
    void closeConnectionLocked(int fd); // 已持锁时调用，不加锁
    void handleRead(int fd);
    void handleWrite(int fd);
    void handleListen();
    void onRequest(int fd);

    void addTimer(int fd, int timeoutMs);
    void extendTimer(int fd, int timeoutMs);

    void stop();

private:
    static Server* instance_;
    static void handleSignal(int sig);

private:
    ServerConfig config_;

    mutable std::mutex mutex_;  // 保护 connections_, epoller_, timer_
    Epoller epoller_;
    Acceptor acceptor_;
    ThreadPool threadPool_;

    std::atomic<bool> stop_{false};

    std::unordered_map<int, HttpConn> connections_;
    HeapTimer timer_;
};

} // namespace ccb
