#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    ThreadPool(size_t threadCount = std::thread::hardware_concurrency());
    ~ThreadPool();

    void submit(std::function<void()> job);

private:
    void workerLoop();

    std::vector<std::thread> workers;

    std::queue<std::function<void()>> jobs;

    std::mutex queueMutex;
    std::condition_variable cv;

    std::atomic<bool> stopping = false;
};