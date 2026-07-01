#include "timer/HeapTimer.h"

#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

using namespace ccb;

#define TEST(name) std::cout << "\n[" << name << "]"

int g_counter = 0;
void cb_counter() { ++g_counter; }

void test_add_and_tick()
{
    TEST("添加定时器并触发");
    g_counter = 0;
    HeapTimer timer;

    // 添加一个 50ms 后到期的定时器
    timer.add(1, 50, cb_counter);
    assert(timer.size() == 1);
    assert(!timer.empty());

    // 还没到期
    timer.tick();
    assert(g_counter == 0);

    // 等待到期
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    timer.tick();
    assert(g_counter == 1);
    assert(timer.empty());

    std::cout << "  PASS" << std::endl;
}

void test_multiple_timers()
{
    TEST("多个定时器按到期顺序触发");
    g_counter = 0;
    HeapTimer timer;

    timer.add(1, 100, []() { g_counter += 1; });   // 100ms
    timer.add(2, 50,  []() { g_counter += 10; });   // 50ms
    timer.add(3, 150, []() { g_counter += 100; });  // 150ms
    assert(timer.size() == 3);

    // 等待 60ms: 只有 id=2 会触发
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    timer.tick();
    assert(g_counter == 10);
    assert(timer.size() == 2);

    // 等待再 50ms: id=1 触发
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer.tick();
    assert(g_counter == 11);
    assert(timer.size() == 1);

    // 等待最后 50ms: id=3 触发
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer.tick();
    assert(g_counter == 111);
    assert(timer.empty());

    std::cout << "  PASS" << std::endl;
}

void test_adjust()
{
    TEST("调整定时器超时时间");
    g_counter = 0;
    HeapTimer timer;

    // 200ms 后到期
    timer.add(1, 200, []() { g_counter = 1; });

    // 调整为 50ms 后到期
    timer.adjust(1, 50);

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    timer.tick();
    assert(g_counter == 1);

    std::cout << "  PASS" << std::endl;
}

void test_delete()
{
    TEST("删除定时器");
    g_counter = 0;
    HeapTimer timer;

    timer.add(1, 50, cb_counter);
    timer.add(2, 100, cb_counter);
    assert(timer.size() == 2);

    timer.del(1);
    assert(timer.size() == 1);

    // 等 id=1 应该到期的时间
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    timer.tick();
    assert(g_counter == 0);  // id=1 已删除

    // 等 id=2 到期
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer.tick();
    assert(g_counter == 1);

    std::cout << "  PASS" << std::endl;
}

void test_clear()
{
    TEST("清空所有定时器");
    HeapTimer timer;

    timer.add(1, 1000, cb_counter);
    timer.add(2, 1000, cb_counter);
    timer.add(3, 1000, cb_counter);

    timer.clear();
    assert(timer.empty());
    assert(timer.size() == 0);

    std::cout << "  PASS" << std::endl;
}

void test_get_next_tick()
{
    TEST("获取最近到期时间");
    HeapTimer timer;

    timer.add(1, 5000, cb_counter);

    int next = timer.getNextTick();
    // 5000ms 到期，剩余时间应该 > 0 且 <= 5000
    assert(next > 0 && next <= 5000);

    timer.add(2, 0, cb_counter);  // 立即到期
    next = timer.getNextTick();
    assert(next == 0);

    std::cout << "  PASS" << std::endl;
}

void test_repeat_add()
{
    TEST("重复添加同一 id 应更新");
    g_counter = 0;
    HeapTimer timer;

    timer.add(1, 5000, cb_counter);  // 5s 后
    timer.add(1, 50, cb_counter);    // 更新为 50ms 后

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    timer.tick();
    assert(g_counter == 1);  // 50ms 版本先触发

    std::cout << "  PASS" << std::endl;
}

int main()
{
    std::cout << "=== HeapTimer 单元测试 ===" << std::endl;

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

    RUN(test_add_and_tick);
    RUN(test_multiple_timers);
    RUN(test_adjust);
    RUN(test_delete);
    RUN(test_clear);
    RUN(test_get_next_tick);
    RUN(test_repeat_add);

    std::cout << "\n================================" << std::endl;
    std::cout << "总计: " << (passed + failed)
              << "  通过: " << passed
              << "  失败: " << failed << std::endl;

    return failed > 0 ? 1 : 0;
}
