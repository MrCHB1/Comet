#pragma once

#include <thread>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>

class ThreadPool
{
public:
	ThreadPool(size_t threadCount = std::thread::hardware_concurrency());
	~ThreadPool();

	void SubmitJob(std::function<void()> job);
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