#include "ThreadPool.h"
#include "engine/EngineCore.h"

namespace parus
{
    ThreadPool::~ThreadPool()
    {
        {
            std::lock_guard lock(queueMutex);
            isPendingStop = true;
        }

        conditionVariable.notify_all();

        for (auto& worker : workers)
        {
            worker.join();
        }
    }

    void ThreadPool::workerJob()
    {
        while (true)
        {
            std::unique_lock lock (queueMutex);
            conditionVariable.wait(lock, [&]()
            {
               return isPendingStop || !tasks.empty(); 
            });

            // Exit condition.
            if (isPendingStop && tasks.empty())
            {
                return;
            }

            std::function<void()> newTask = std::move(tasks.front());
            tasks.pop();
            lock.unlock();

            newTask();
        }
    }

    void ThreadPool::init(const unsigned int numberOfThreads)
    {
        LOG_INFO("Initializing Thread Pool with " + std::to_string(numberOfThreads) + " threads.");
        for (unsigned int i = 0; i < numberOfThreads; ++i)
        {
            workers.emplace_back(&ThreadPool::workerJob, this);
        }
    }

    void ThreadPool::enqueue(const std::function<void()>& task)
    {
        std::unique_lock lock (queueMutex);
        tasks.emplace(task);
        conditionVariable.notify_all();
    }

    bool ThreadPool::isBusy() const
    {
        std::lock_guard lock(queueMutex);
        return !tasks.empty();
    }
}
