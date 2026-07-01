#include "pool/sqlconnpool/SqlConnPool.h"
#include "pool/sqlconnpool/Connection.h"

#include <iostream>
#include <cassert>
#include <string>
#include <thread>
#include <vector>
#include <chrono>

using namespace ccb;

#define TEST(name) std::cout << "\n[" << name << "]"

// 测试连接参数（需要 MySQL 运行且有 webserver 数据库）
static const std::string DB_HOST = "127.0.0.1";
static const std::string DB_USER = "ccb";
static const std::string DB_PWD  = "123456";
static const unsigned int DB_PORT = 3306;
static const std::string DB_NAME = "webserver";

void test_single_connection()
{
    TEST("单次直接连接");
    MysqlConnection conn;
    bool ok = conn.connect(DB_HOST, DB_USER, DB_PWD, DB_PORT, DB_NAME);
    assert(ok);

    // 测试查询
    MYSQL_RES* res = conn.query("SELECT 1 AS test");
    assert(res != nullptr);

    MYSQL_ROW row = mysql_fetch_row(res);
    assert(row != nullptr);
    assert(std::string(row[0]) == "1");

    mysql_free_result(res);
    std::cout << "  PASS" << std::endl;
}

void test_escape_string()
{
    TEST("SQL 转义防注入");
    MysqlConnection conn;
    conn.connect(DB_HOST, DB_USER, DB_PWD, DB_PORT, DB_NAME);

    std::string escaped = conn.escapeString("test' OR '1'='1");
    assert(escaped.find("\\'") != std::string::npos);

    std::cout << "  PASS" << std::endl;
}

void test_pool_init()
{
    TEST("连接池初始化");
    SqlConnectionPool* pool = SqlConnectionPool::getInstance();
    pool->init(DB_HOST, DB_USER, DB_PWD, DB_PORT, DB_NAME,
               2,     // minSize
               4,     // maxSize
               300,   // maxIdleTime
               1000); // connectionTimeout
    pool->start();

    // 获取第一个连接
    auto conn1 = pool->getConnection();
    assert(conn1 != nullptr);
    std::cout << "  获取连接成功" << std::endl;

    // 获取第二个连接
    auto conn2 = pool->getConnection();
    assert(conn2 != nullptr);
    std::cout << "  第二个连接成功" << std::endl;

    std::cout << "  PASS" << std::endl;
}

void test_pool_reuse()
{
    TEST("连接复用（归还后重新获取）");
    SqlConnectionPool* pool = SqlConnectionPool::getInstance();

    // 获取连接，执行查询，归还
    int result = 0;
    {
        auto conn = pool->getConnection();
        assert(conn != nullptr);

        MYSQL_RES* res = conn->query("SELECT 42 AS val");
        assert(res != nullptr);
        MYSQL_ROW row = mysql_fetch_row(res);
        result = std::stoi(row[0]);
        mysql_free_result(res);
    } // conn 离开作用域，归还到池

    assert(result == 42);

    // 再次获取，应该是同一个或另一个可用连接
    {
        auto conn = pool->getConnection();
        assert(conn != nullptr);
        MYSQL_RES* res = conn->query("SELECT 1");
        assert(res != nullptr);
        mysql_free_result(res);
    }

    std::cout << "  PASS" << std::endl;
}

void test_pool_concurrent()
{
    TEST("并发获取连接");
    SqlConnectionPool* pool = SqlConnectionPool::getInstance();

    const int N = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success{0};

    for (int i = 0; i < N; ++i)
    {
        threads.emplace_back([pool, &success]() {
            auto conn = pool->getConnection();
            if (conn)
            {
                MYSQL_RES* res = conn->query("SELECT 1");
                if (res)
                {
                    mysql_free_result(res);
                    ++success;
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    assert(success.load() == N);
    std::cout << "  并发 " << N << " 个连接全部成功" << std::endl;
    std::cout << "  PASS" << std::endl;
}

int main()
{
    std::cout << "=== SQL 连接池单元测试 ===" << std::endl;
    std::cout << "注意: 需要 MySQL 运行且 webserver 数据库存在" << std::endl;

    int passed = 0, failed = 0;

#define RUN(test)                        \
    do                                    \
    {                                     \
        try                                \
        {                                  \
            test();                       \
            ++passed;                      \
        }                                  \
        catch (const std::exception &e)   \
        {                                  \
            std::cerr << "  FAIL: " << e.what() << std::endl; \
            ++failed;                      \
        }                                  \
        catch (...)                        \
        {                                  \
            std::cerr << "  FAIL: unknown" << std::endl; \
            ++failed;                      \
        }                                  \
    } while (0)

    RUN(test_single_connection);
    RUN(test_escape_string);
    RUN(test_pool_init);
    RUN(test_pool_reuse);
    RUN(test_pool_concurrent);

    std::cout << "\n================================" << std::endl;
    std::cout << "总计: " << (passed + failed)
              << "  通过: " << passed
              << "  失败: " << failed << std::endl;

    return failed > 0 ? 1 : 0;
}
