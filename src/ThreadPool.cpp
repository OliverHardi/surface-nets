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
    activeJobs++;
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
        activeJobs--;
    }
}

void ThreadPool::shutdown()
{
    {
        std::lock_guard lock(queueMutex);
        stopping = true;

        // Drop anything not yet started — don't drain the backlog
        std::queue<std::function<void()>> empty;
        std::swap(jobs, empty);
    }

    cv.notify_all();

    for (auto& thread : workers)
    {
        if (thread.joinable())
            thread.join();
    }

    workers.clear();
}
