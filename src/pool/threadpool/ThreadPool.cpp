#include "pool/threadpool/ThreadPool.h"
#include "log/Logger.h"

namespace ccb
{

ThreadPool::ThreadPool(size_t threadsMinSize, 
	size_t threadsMaxSize,
	size_t taskQueMaxSize, 
	ThreadPoolMode mode, 
	bool isRunning, 
	RejectPolicy rejectPolicy, 
	std::chrono::milliseconds idleTimeout)
	: threadsMinSize_(threadsMinSize), 
	  threadsMaxSize_(threadsMaxSize), 
	  taskQueMaxSize_(taskQueMaxSize),
	  mode_(mode),
	  isRunning_(isRunning),
	  rejectPolicy_(rejectPolicy),
	  idleTimeout_(idleTimeout)
{
	// 基础参数校验，避免创建无意义或无法正确工作的线程池。
    if (threadsMinSize_ == 0)
    {
        throw std::invalid_argument("threadsMinSize must be greater than 0");
    }

    if (threadsMaxSize_ < threadsMinSize_)
    {
        throw std::invalid_argument("threadsMaxSize must be greater than or equal to threadsMinSize");
    }

    if (taskQueMaxSize_ == 0)
    {
        throw std::invalid_argument("taskQueMaxSize must be greater than 0");
    }

	if (idleTimeout_.count() < 0) 
	{
    	throw std::invalid_argument("idleTimeout must be non-negative");
	}

	// 创建工作线程
	for (int i = 0; i < threadsMinSize_; ++i)
	{
		addWorkerThread();
	}

	LOG_INFO("ThreadPool created: min=%zu, max=%zu, queue=%zu, mode=%s",
		threadsMinSize_, threadsMaxSize_, taskQueMaxSize_,
		mode_ == ThreadPoolMode::FIXED ? "FIXED" : "CACHED");
}

ThreadPool::~ThreadPool()
{	
	
	std::unique_lock<std::mutex> lock(mtx_);
	isRunning_ = false;
	notEmpty_.notify_all();
	notFull_.notify_all();

	cvExit_.wait(lock, [this]() ->bool {
		return workers_.empty();
	});

}

void ThreadPool::addWorkerThread()
{
	std::size_t id = ++nextWorkerId_;
	std::thread t(&ThreadPool::threadFunc, this, id);
	t.detach();
	workers_[id] = std::make_unique<Worker>(id, std::move(t));
	++curThreadCount_;
	LOG_DEBUG("Worker thread created id=%zu, total=%zu", id, curThreadCount_.load());
}

void ThreadPool::expandCachedThreadsLocked()
{
	if(mode_ != ThreadPoolMode::CACHED)
		return;
	
	while(isRunning_ && 
		taskQue_.size() > idleThreadCount_.load() * EXPAND_FACTOR && 
		curThreadCount_.load() < threadsMaxSize_)
	{
		addWorkerThread();
	}
	
}

void ThreadPool::threadFunc(std::size_t workerId)
{
	while (true)
	{
		Task task = nullptr;
		{
			std::unique_lock<std::mutex> lock(mtx_);
			++idleThreadCount_;

			if(mode_ == ThreadPoolMode::CACHED)
			{
				// CACHED 模式不会永久等待：超时后如果线程数量超过最小值，就回收自己。
				bool awakened = notEmpty_.wait_for(lock, idleTimeout_, [this]() -> bool {
					return !taskQue_.empty() || !isRunning_;
				});

				--idleThreadCount_;

				if(!awakened)
				{
					if(isRunning_ && curThreadCount_.load() > threadsMinSize_)
					{
						// 删除这个线程
                        workers_.erase(workerId);
						--curThreadCount_;
                        cvExit_.notify_one();
						LOG_DEBUG("Cached worker thread id=%zu exiting (idle timeout), remaining=%zu",
                            workerId, curThreadCount_.load());		
						return;
					}

					continue;
				}
			}
			else
			{
				notEmpty_.wait(lock, [&]() -> bool {
				return !taskQue_.empty() || !isRunning_;
				});

				--idleThreadCount_;
			}

			// 线程池关闭且没有任务
			if (!isRunning_ && taskQue_.empty())
			{
                workers_.erase(workerId);
                --curThreadCount_;
                cvExit_.notify_one();
				LOG_DEBUG("Worker thread id=%zu exiting (pool stopped), remaining=%zu",
                    workerId, curThreadCount_.load());
				return;
			}

			LOG_DEBUG("Worker thread id=%zu picking up task, queue size=%zu",
                workerId, taskQue_.size());
			task = std::move(taskQue_.front());
			taskQue_.pop();

			++activeThreadCount_;
			notFull_.notify_one();
		}


		try 
		{
			task();
		} 
		catch (const std::exception& e) 
		{
			LOG_ERROR("Task execution failed in worker id=%zu: %s", workerId, e.what());
		} 
		catch (...) 
		{
			LOG_ERROR("Task execution failed in worker id=%zu: unknown exception", workerId);
		}
			
		--activeThreadCount_;
	}
}

} // namespace ccb
