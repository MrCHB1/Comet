#pragma once

#include <thread>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>

class ThreadPool
{
public:
	ThreadPool(size_t threadCount = std::thread::hardware_concurrency());
	~ThreadPool();

	void SubmitJob(std::function<void()> job);
	std::future<void> SubmitJobWithFuture(std::function<void()> job);
	size_t WorkerCount() const { return workers.size(); }
	void WaitForAllJobs();

private:
	void WorkerLoop();

	std::vector<std::thread> workers;
	std::queue<std::function<void()>> jobs;

	std::mutex queueMutex;
	std::condition_variable queueCV;
	std::condition_variable finishedCV;

	size_t activeJobs = 0;
	bool stopping = false;
};