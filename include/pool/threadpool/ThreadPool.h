#pragma once

#include <queue>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

#include "common/Noncopyable.h"
#include "pool/threadpool/Worker.h"

namespace ccb
{

using Task = std::function<void()>;

// 默认配置：最小线程数、最大线程数和任务队列容量
const size_t THREAD_MIN_SIZE = 2;
const size_t THREAD_MAX_SIZE = 64;
const size_t TASK_MAX_SIZE = 1024;

/**
 * 线程池运行模型：
 * FIXED：固定线程数量，不自动扩容或缩容
 * CACHED：根据任务压力动态扩容，空闲线程超时后自动回收
 */
enum class ThreadPoolMode
{
	FIXED,
	CACHED
};

/**
 * 任务队列满时的拒绝策略：
 * 
 */

enum class RejectPolicy
{
	THROW,			// 直接抛出异常
	BLOCK,			// 阻塞提交线程，直到队列有空位
	CALLER_RUNS,	// 由提交任务的线程同步执行该任务
	DISCARD,		// 拒绝新任务并抛出异常
	DISCARD_OLDEST	// 丢弃队列中最旧的任务，再加入新任务

};

class ThreadPool : private Noncopyable
{
public:
	explicit ThreadPool(size_t threadsMinSize = THREAD_MIN_SIZE, 
		size_t threadsMaxSize = THREAD_MAX_SIZE, 
		size_t taskQueMaxSize = TASK_MAX_SIZE, 
		ThreadPoolMode mode = ThreadPoolMode::FIXED, 
		bool isRunning = true, 
		RejectPolicy rejectPolicy = RejectPolicy::DISCARD, 
		std::chrono::milliseconds idleTimeout = std::chrono::milliseconds(60000));

	~ThreadPool();

	template <typename F, typename... Args>
	auto submitTask(F&& f, Args&&... args) 
	-> std::future<std::invoke_result_t<F, Args...>>;

	static const int EXPAND_FACTOR = 2;
	
private:
	void threadFunc(std::size_t id);
	void addWorkerThread();
	void expandCachedThreadsLocked();

private:
	size_t threadsMinSize_;		// 线程池中最小线程数量
	size_t threadsMaxSize_;		// 线程池中最大线程数量
	std::unordered_map<std::size_t, std::unique_ptr<Worker>> workers_;		// 工作线程
	std::atomic_size_t idleThreadCount_{0};		// 空闲线程数量
	std::atomic_size_t curThreadCount_{0};		// 当前线程数量
	std::atomic_size_t activeThreadCount_{0};	// 工作中的线程数量
	std::atomic_size_t nextWorkerId_{0};		// 工作线程对应id

	size_t taskQueMaxSize_;		// 任务队列中任务最大数量
	std::queue<Task> taskQue_;	// 任务队列

	
	std::mutex mtx_;
	std::condition_variable notEmpty_;
	std::condition_variable notFull_;
	std::condition_variable cvExit_;

	std::atomic_bool isRunning_;

	ThreadPoolMode mode_;
	RejectPolicy rejectPolicy_;					// 拒绝策略
	std::chrono::milliseconds idleTimeout_;		// 空闲时间

};

template <typename F, typename... Args>
auto ThreadPool::submitTask(F&& f, Args&&... args) 
	-> std::future<std::invoke_result_t<F, Args...>>
{
	using ReturnType = std::invoke_result_t<F, Args...>;
	
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<ReturnType> result = task->get_future();
	bool runInCaller = false;

    {
        std::unique_lock<std::mutex> lock(mtx_);

        if (!isRunning_)
        {
            throw std::runtime_error("submitTask failed: thread pool stopped");
        }

        if (taskQue_.size() >= taskQueMaxSize_)
        {
			switch (rejectPolicy_)
			{
			case RejectPolicy::BLOCK:
				notFull_.wait(lock, [this]() ->bool{
					return taskQue_.size() < taskQueMaxSize_ || !isRunning_;
				});

				if(!isRunning_)
				{
					throw std::runtime_error("submitTask failed: thread pool stopped");
				}
				break;
			
			case RejectPolicy::CALLER_RUNS:
				runInCaller = true;
				break;
			
			case RejectPolicy::DISCARD_OLDEST:
				// 丢弃最早入队但尚未执行的任务，为新任务腾出一个位置
				if(!taskQue_.empty())
				{
					taskQue_.pop();
					notFull_.notify_one();
				}

				if(!isRunning_)
				{
					throw std::runtime_error("submitTask failed: thread pool stopped");
				}
				break;
			case RejectPolicy::DISCARD:
			case RejectPolicy::THROW:
			default:
				throw std::runtime_error("submitTask failed: task queue full");
				break;
			}
        }

		if(!runInCaller)
		{
			taskQue_.emplace([task]() {
				(*task)();
			});

            // CACHED 模式下，根据当前队列压力决定是否扩容。
            expandCachedThreadsLocked();
		}
    }

	if(runInCaller)
	{
		// CALLER_RUNS 策略下，任务已经不在线程池里执行，需要在这里同步完成。
		(*task)();
	}
	else
	{
		notEmpty_.notify_one();
	}

    return result;
}

} // namespace ccb
