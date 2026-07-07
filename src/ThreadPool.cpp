#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t threadCount)
{
    if (threadCount == 0)
        threadCount = 1;

    for (size_t i = 0; i < threadCount; i++)
    {
        workers.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool()
{
    stopping = true;
    cv.notify_all();

    for (auto& thread : workers)
    {
        if (thread.joinable())
            thread.join();
    }
}

void ThreadPool::submit(std::function<void()> job)
{
    {
        std::lock_guard lock(queueMutex);
        jobs.push(std::move(job));
    }

    cv.notify_one();
}

void ThreadPool::workerLoop()
{
    while (true)
    {
        std::function<void()> job;

        {
            std::unique_lock lock(queueMutex);

            cv.wait(lock, [&]
            {
                return stopping || !jobs.empty();
            });

            if (stopping && jobs.empty())
                return;

            job = std::move(jobs.front());
            jobs.pop();
        }

        job();
    }
}