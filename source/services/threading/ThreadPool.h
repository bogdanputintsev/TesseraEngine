#pragma once
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "services/Service.h"

namespace parus
{
    
    class ThreadPool final : public Service
    {
    public:
        ThreadPool() = default;
        ~ThreadPool();
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&&) = delete;

        static constexpr unsigned int DEFAULT_NUMBER_OF_THREADS = 3;
        
        void init(const unsigned int numberOfThreads = DEFAULT_NUMBER_OF_THREADS);
        void enqueue(const std::function<void()>& task);
        
        [[nodiscard]] bool isBusy() const;
    private:
        
        void workerJob();
        
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        mutable std::mutex queueMutex;
        std::condition_variable conditionVariable;
        bool isPendingStop = false;
    };

#define RUN_ASYNC(function) Services::get<ThreadPool>()->enqueue([&]{function})
    
}
