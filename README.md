# Nexus HttpServer

[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Linux](https://img.shields.io/badge/Linux-Ubuntu%2024.04-orange.svg)](https://ubuntu.com/)
[![License](https://img.shields.io/badge/License-GPL%202.0-green.svg)](LICENSE)

基于 **Reactor + 线程池** 的高性能 C++17 HTTP 服务器，支持静态文件零拷贝传输、用户注册登录、MySQL 连接池，单机 QPS 破万。

## 🏗 架构

```
                   ┌──────────┐
                   │  Client   │
                   └────┬─────┘
                        │
                        ▼
┌─────────────────  Epoller (epoll + ET) ──────────────────┐
│                                                            │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │  Accept  │  │   Read   │  │  Write   │  │  Timer   │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘  │
│       │              │             │              │        │
│       │         ┌────▼────┐        │              │        │
│       │         │  Task   │        │              │        │
│       │         │ Queue   │        │              │        │
│       │         └────┬────┘        │              │        │
│       │              │             │              │        │
│       │    ┌─────────▼─────────┐   │              │        │
│       │    │   ThreadPool     │   │              │        │
│       │    │ ┌───┬───┬───┬───┐│   │              │        │
│       │    │ │ W │ W │ W │ W ││   │              │        │
│       │    │ └───┴───┴───┴───┘│   │              │        │
│       │    └─────────┬─────────┘   │              │        │
│       │              │             │              │        │
│       │    ┌─────────▼─────────┐   │              │        │
│       │    │  HttpConn        │   │              │        │
│       │    │  ┌─────────────┐ │   │              │        │
│       │    │  │ HttpParser  │ │   │              │        │
│       │    │  ├─────────────┤ │   │              │        │
│       │    │  │ HttpHandler │─┼───┼── SqlConnPool │
│       │    │  ├─────────────┤ │   │              │        │
│       │    │  │ sendfile    │ │   │              │        │
│       │    │  └─────────────┘ │   │              │        │
│       │    └───────────────────┘   │              │        │
└────────────────────────────────────────────────────────────┘
```

## 🚀 性能

测试环境：VMware 虚拟机 4 核 8GB，服务端与压测端分离。

| 场景 | 并发 | QPS | 平均延迟 | 吞吐量 |
|------|------|-----|---------|--------|
| 静态首页 (5KB) | 100 | 13,371 | 7.6ms | 64 MB/s |
| 静态首页 (5KB) | 500 | 14,032 | 37.5ms | 67 MB/s |
| 静态首页 (5KB) | 1000 | 10,326 | 115ms | 49 MB/s |
| 大文件 (38MB) | 5 | 11.26 | 352ms | 428 MB/s |

---

## ✨ 特性

### 核心
- **epoll + ET 模式** — 边缘触发 I/O 多路复用
- **Reactor 单线程事件循环** — accept / read / write 由主线程驱动
- **多线程并行处理** — 工作线程池处理 HTTP 解析与业务逻辑，主线程无阻塞
- **处理阶段去锁设计** — 请求处理阶段不持全局锁，4 个工作线程真正并行

### 网络
- **sendfile 零拷贝** — 静态文件直接内核发送，38MB 大文件加载从 1.6s → 50ms
- **HTTP/1.1 Keep-Alive** — 长连接复用，减少 TCP 握手开销
- **边缘触发批量 accept** — 一次事件中循环接受所有并发连接
- **大小写不敏感头解析** — 兼容不同浏览器的 HTTP 头格式

### 线程池
- **FIXED / CACHED 双模式** — 固定线程数 or 弹性扩缩容
- **4 种拒绝策略** — THROW / BLOCK / CALLER_RUNS / DISCARD
- **空闲超时回收** — CACHED 模式下空闲线程自动退出

### 数据库
- **MySQL 连接池** — 单例，生产者-消费者模型
- **空闲连接扫描回收** — 定时清理超时闲置连接
- **SQL 注入防护** — `mysql_real_escape_string()` 转义
- **SHA2 密码哈希** — 数据库侧 SHA2-256 哈希，不传明文

### HTTP 协议
- 手写 HTTP/1.1 解析器（请求行 → 头部 → 包体 状态机）
- GET / POST / HEAD / PUT / DELETE 方法
- URL 解码、表单解析
- `Content-Type` 自动推断
- `Cache-Control` 缓存头
- 错误状态码：400 / 404 / 405 / 500

### 日志
- 异步双缓冲日志，DEBUG / INFO / WARN / ERROR / FATAL 五级
- 自动滚动切分，按文件大小

---

## 📦 编译

```bash
# 前置依赖
sudo apt install -y cmake build-essential libmysqlclient-dev

# 构建
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## ⚙ 配置

编辑 `config/server.yaml`：

```yaml
# 服务器
port = 8080                     # 监听端口
trigMode = 3                    # 触发模式: 0=LT+LT, 1=ET+LT, 2=LT+ET, 3=ET+ET
timeoutMs = 60000               # 连接超时(ms)

# 日志
enableLog = true                # 是否启用
logLevel = 0                    # 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=FATAL
logRollSize = 104857600         # 单文件最大 100MB

# 线程池
threadMinSize = 4               # 最小线程数
threadMaxSize = 4               # 最大线程数
taskQueueMaxSize = 4096         # 任务队列容量
threadPoolMode = 0              # 0=FIXED, 1=CACHED
threadRejectPolicy = 1          # 0=THROW, 1=BLOCK, 2=CALLER_RUNS, 3=DISCARD, 4=DISCARD_OLDEST
threadIdleTimeoutMs = 60000     # 空闲线程超时

# MySQL 连接池
enableSqlPool = true
sqlHost = 127.0.0.1
sqlPort = 3306
sqlUser = ccb
sqlPwd  = 123456
sqlDbName = webserver
sqlMinSize = 4
sqlMaxSize = 32
sqlMaxIdleTime = 300
sqlConnectionTimeout = 1000
```

---

## 🖥 运行

```bash
# 1. 初始化数据库
mysql -u root -p < init.sql

# 2. 启动服务
./build/httpserver

# 浏览器访问
http://localhost:8080/
```

---

## 📊 性能优化过程

| 阶段 | 优化项 | 效果 |
|------|--------|------|
| 原始 | — | 19并发 15个超时，视频 1.6s |
| 第一轮 | ET批量accept + Content-Type映射 + `.html`后缀补全 | 19/19成功，37ms |
| 第二轮 | sendfile 零拷贝 | 视频 1.6s → 50ms (32×) |
| 第三轮 | 处理阶段去锁 + 直写响应 + Cache-Control | 37ms → 8ms (4.6×) |
| 第四轮 | 任务队列扩容 + BLOCK策略 | 1000并发 109QPS → 10326QPS |

---

## 🧪 测试

```bash
# 单元测试
cd test
g++ -o test_buffer test_buffer.cpp ../src/net/Buffer.cpp -I../include -std=c++17
./test_buffer

# 压力测试
wrk -t4 -c100 -d30s http://192.168.111.100:8080/
wrk -t4 -c500 -d30s http://192.168.111.100:8080/
wrk -t4 -c5 -d30s --timeout 60s http://192.168.111.100:8080/video/xxx.mp4
```

---

## 📁 项目结构

```
HttpServer/
├── include/          # 头文件
│   ├── common/       # Config, Noncopyable
│   ├── http/         # HttpConn, HttpParser, HttpHandler, HttpRequest, HttpResponse
│   ├── log/          # Logger, AsyncLogger, LogFile, BlockQueue
│   ├── net/          # Epoller, Acceptor, Socket, Buffer, InetAddress
│   ├── pool/
│   │   ├── threadpool/   # ThreadPool, Worker
│   │   └── sqlconnpool/  # SqlConnPool, Connection
│   ├── server/       # Server
│   └── timer/        # HeapTimer
├── src/              # 源文件 (21个 .cpp)
├── config/           # 配置文件
├── webroot/          # 前端静态资源
├── test/             # 单元测试
├── CMakeLists.txt
└── init.sql          # 数据库初始化
```

---

## 🔑 技术栈

- **I/O**: epoll (ET mode), sendfile, non-blocking socket
- **并发**: POSIX thread, std::mutex, std::condition_variable
- **数据库**: MySQL C API, connection pool
- **日志**: 异步双缓冲, spdlog 风格宏接口
- **构建**: CMake + C++17
- **前端**: Bootstrap + jQuery

---

## 📝 License

GPL 2.0
