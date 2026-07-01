#include "pool/threadpool/ThreadPool.h"

#include <iostream>
#include <cassert>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <future>

using namespace ccb;

// ============================================================
// 辅助工具
// ============================================================
std::mutex g_logMutex;
#define LOG(msg)                                      \
    do                                                 \
    {                                                  \
        std::lock_guard<std::mutex> _lk(g_logMutex);   \
        std::cout << msg << std::endl;                \
    } while (0)

#define TEST(name) LOG("\n===== " << name << " =====")

int square(int x) { return x * x; }

int faultyTask()
{
    throw std::runtime_error("intentional exception");
    return -1;
}

// ============================================================
// 同步慢任务辅助函数
// 提交后会阻塞直到线程开始执行该任务，确保后续 submitTask
// 一定看到队列满，消除时序竞态。
// ============================================================
struct SyncTask
{
    std::mutex mtx;
    std::condition_variable cv;
    bool started = false;

    // 返回一个 lambda，线程执行它时会通知调用者
    auto occupyThread(int sleepMs)
    {
        return [this, sleepMs]() -> int {
            {
                std::lock_guard<std::mutex> lock(mtx);
                started = true;
            }
            cv.notify_one();
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            return 0;
        };
    }

    // 等待线程开始执行任务
    void waitForStart()
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return started; });
    }
};

// ============================================================
// 用例 1：FIXED 模式 — 基础任务提交与返回值
// ============================================================
void testFixedBasic()
{
    TEST("FIXED 模式 - 基础任务提交与返回值");
    ThreadPool pool(2, 2, 8, ThreadPoolMode::FIXED, true, RejectPolicy::THROW);

    auto f1 = pool.submitTask(square, 5);
    auto f2 = pool.submitTask(square, 10);
    auto f3 = pool.submitTask(square, 20);

    assert(f1.get() == 25);
    assert(f2.get() == 100);
    assert(f3.get() == 400);

    LOG("  PASS");
}

// ============================================================
// 用例 2：FIXED 模式 + THROW 拒绝策略
// ============================================================
void testFixedThrow()
{
    TEST("FIXED 模式 - THROW 拒绝策略");
    ThreadPool pool(1, 1, 1, ThreadPoolMode::FIXED, true, RejectPolicy::THROW);

    SyncTask sync;
    pool.submitTask(sync.occupyThread(300)); // 占住线程
    sync.waitForStart();                      // 确保线程已在执行

    pool.submitTask([]() -> int { return 0; }); // T2: 进队列（满）

    try
    {
        pool.submitTask([]() -> int { return 0; }); // T3: 满 → THROW
        LOG("  FAIL: 期望 THROW 异常但未抛出");
        assert(false);
    }
    catch (const std::runtime_error &e)
    {
        std::cout << "  捕获: " << e.what() << std::endl;
        LOG("  PASS");
    }
}

// ============================================================
// 用例 3：FIXED 模式 + BLOCK 拒绝策略
// ============================================================
void testFixedBlock()
{
    TEST("FIXED 模式 - BLOCK 拒绝策略");
    ThreadPool pool(1, 1, 1, ThreadPoolMode::FIXED, true, RejectPolicy::BLOCK);

    SyncTask sync;
    auto f1 = pool.submitTask(sync.occupyThread(300));
    sync.waitForStart();

    pool.submitTask([]() -> int { return 2; });    // T2: 进队列（满）

    auto f3 = pool.submitTask([]() -> int { return 3; }); // T3: BLOCK → 等待

    assert(f1.get() == 0);
    assert(f3.get() == 3);

    LOG("  PASS (BLOCK 等待后成功入队并执行)");
}

// ============================================================
// 用例 4：FIXED 模式 + CALLER_RUNS 拒绝策略
// ============================================================
void testFixedCallerRuns()
{
    TEST("FIXED 模式 - CALLER_RUNS 拒绝策略");
    ThreadPool pool(1, 1, 1, ThreadPoolMode::FIXED, true, RejectPolicy::CALLER_RUNS);

    SyncTask sync;
    pool.submitTask(sync.occupyThread(300));
    sync.waitForStart();

    pool.submitTask([]() -> int { return 0; });    // T2: 进队列（满）

    auto callerId = std::this_thread::get_id();
    auto f3 = pool.submitTask([callerId]() -> int { // T3: CALLER_RUNS
        return std::this_thread::get_id() == callerId ? 42 : -1;
    });

    assert(f3.get() == 42);
    LOG("  PASS (任务由调用者线程同步执行)");
}

// ============================================================
// 用例 5：FIXED 模式 + DISCARD_OLDEST 拒绝策略
// ============================================================
void testFixedDiscardOldest()
{
    TEST("FIXED 模式 - DISCARD_OLDEST 拒绝策略");
    ThreadPool pool(1, 1, 1, ThreadPoolMode::FIXED, true, RejectPolicy::DISCARD_OLDEST);

    SyncTask sync;
    pool.submitTask(sync.occupyThread(300));
    sync.waitForStart();

    // T2: 进队列（满，最旧的）
    auto f2 = pool.submitTask([]() -> int { return 2; });

    // T3: 满 → 丢弃 T2（broken_promise），入队 T3
    auto f3 = pool.submitTask([]() -> int { return 3; });

    try
    {
        f2.get();
        LOG("  FAIL: 期望丢弃的任务抛出 broken_promise");
        assert(false);
    }
    catch (const std::future_error &e)
    {
        std::cout << "  丢弃的 future: " << e.what() << std::endl;
    }

    assert(f3.get() == 3);
    LOG("  PASS");
}

// ============================================================
// 用例 6：FIXED 模式 + DISCARD 拒绝策略
// ============================================================
void testFixedDiscard()
{
    TEST("FIXED 模式 - DISCARD 拒绝策略");
    ThreadPool pool(1, 1, 1, ThreadPoolMode::FIXED, true, RejectPolicy::DISCARD);

    SyncTask sync;
    pool.submitTask(sync.occupyThread(300));
    sync.waitForStart();

    pool.submitTask([]() -> int { return 0; });

    try
    {
        pool.submitTask([]() -> int { return 3; });
        LOG("  FAIL: 期望 DISCARD 异常");
        assert(false);
    }
    catch (const std::runtime_error &e)
    {
        std::cout << "  捕获: " << e.what() << std::endl;
        LOG("  PASS");
    }
}

// ============================================================
// 用例 7：异常传播
// ============================================================
void testExceptionPropagation()
{
    TEST("异常传播 - 任务异常通过 future.get() 重新抛出");
    ThreadPool pool(1, 1, 4, ThreadPoolMode::FIXED, true, RejectPolicy::THROW);

    auto f = pool.submitTask(faultyTask);
    try
    {
        f.get();
        LOG("  FAIL: 期望异常");
        assert(false);
    }
    catch (const std::runtime_error &e)
    {
        std::cout << "  捕获: " << e.what() << std::endl;
        LOG("  PASS");
    }
}

// ============================================================
// 用例 8：Lambda 与函数对象
// ============================================================
void testLambdaAndFunctor()
{
    TEST("Lambda 与函数对象");
    ThreadPool pool(2, 2, 8, ThreadPoolMode::FIXED, true, RejectPolicy::THROW);

    auto f1 = pool.submitTask([](int a, int b) { return a + b; }, 3, 4);
    assert(f1.get() == 7);

    int x = 10;
    auto f2 = pool.submitTask([x]() mutable {
        x += 5;
        return x;
    });
    assert(f2.get() == 15);

    LOG("  PASS");
}

// ============================================================
// 用例 9：CACHED 模式 — 动态扩容 + 空闲回收
// ============================================================
void testCachedExpandAndRecycle()
{
    TEST("CACHED 模式 - 动态扩容与空闲回收");
    // 使用 BLOCK 策略，避免队列短暂填满时抛出异常
    // 最少 1 线程，最多 4 线程，队列容量 4，空闲超时 500ms
    ThreadPool pool(1, 4, 4, ThreadPoolMode::CACHED, true, RejectPolicy::BLOCK,
                    std::chrono::milliseconds(500));

    constexpr int N = 6;
    std::vector<std::future<int>> futures;
    for (int i = 0; i < N; ++i)
    {
        futures.push_back(pool.submitTask([](int v) -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return v;
        }, i + 1));
    }

    for (int i = 0; i < N; ++i)
    {
        assert(futures[i].get() == i + 1);
    }

    LOG("  扩容完成，" << N << " 个任务全部执行完毕");
    LOG("  PASS");
}

// ============================================================
// 用例 10：CACHED 压力测试
// ============================================================
void testStress()
{
    TEST("CACHED 压力测试 - 批量提交");
    ThreadPool pool(4, 8, 32, ThreadPoolMode::CACHED, true, RejectPolicy::BLOCK,
                    std::chrono::milliseconds(1000));

    constexpr int N = 50;
    std::vector<std::future<int>> futures;
    futures.reserve(N);

    for (int i = 0; i < N; ++i)
    {
        futures.push_back(pool.submitTask([](int v) { return v * 2; }, i));
    }

    for (int i = 0; i < N; ++i)
    {
        assert(futures[i].get() == i * 2);
    }

    LOG("  全部 " << N << " 个任务执行完毕");
    LOG("  PASS");
}

// ============================================================
// 用例 11：优雅退出 — 析构时仍有任务在执行
// ============================================================
void testGracefulShutdown()
{
    TEST("优雅退出 - 析构时任务未完成");
    {
        ThreadPool pool(2, 2, 8, ThreadPoolMode::FIXED, true, RejectPolicy::THROW);

        pool.submitTask([]() -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            return 42;
        });
        // 不等待结果，直接析构
    }
    LOG("  PASS (析构未死锁)");
}

// ============================================================
// 主函数
// ============================================================
int main()
{
    std::cout << "=== C++ 线程池测试套件 ===" << std::endl;

    int passed = 0;
    int failed = 0;

#define RUN(test)                                \
    do                                           \
    {                                            \
        try                                      \
        {                                        \
            test();                              \
            ++passed;                            \
        }                                        \
        catch (const std::exception &e)          \
        {                                        \
            LOG("  FAIL (异常): " << e.what());   \
            ++failed;                            \
        }                                        \
        catch (...)                              \
        {                                        \
            LOG("  FAIL (未知异常)");             \
            ++failed;                            \
        }                                        \
    } while (0)

    RUN(testFixedBasic);
    RUN(testFixedThrow);
    RUN(testFixedBlock);
    RUN(testFixedCallerRuns);
    RUN(testFixedDiscardOldest);
    RUN(testFixedDiscard);
    RUN(testExceptionPropagation);
    RUN(testLambdaAndFunctor);
    RUN(testCachedExpandAndRecycle);
    RUN(testStress);
    RUN(testGracefulShutdown);

    std::cout << "\n========================" << std::endl;
    std::cout << "总计: " << (passed + failed)
              << "  通过: " << passed
              << "  失败: " << failed << std::endl;
    std::cout << "========================" << std::endl;

    return failed > 0 ? 1 : 0;
}
