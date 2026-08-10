#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t threadCount)
{
	if (threadCount == 0) threadCount = 1;

	for (size_t i = 0; i < threadCount; i++)
	{
		workers.emplace_back([this] { WorkerLoop(); });
	}
}

ThreadPool::~ThreadPool()
{
	{
		std::lock_guard<std::mutex> lock(queueMutex);
		stopping = true;
	}

	queueCV.notify_all();

	for (std::thread& worker : workers)
		worker.join();
}

void ThreadPool::SubmitJob(std::function<void()> job)
{
	{
		std::lock_guard lock(queueMutex);
		jobs.push(std::move(job));
	}

	queueCV.notify_one();
}

void ThreadPool::WaitForAllJobs()
{
	std::unique_lock<std::mutex> lock(queueMutex);

	finishedCV.wait(lock, [this]
		{
			return jobs.empty() && activeJobs == 0;
		});
}

void ThreadPool::WorkerLoop()
{
	while (true)
	{
		std::function<void()> job;

		{
			std::unique_lock lock(queueMutex);

			queueCV.wait(lock, [this]
				{
					return stopping || !jobs.empty();
				});

			if (stopping && jobs.empty()) return;

			job = std::move(jobs.front());
			jobs.pop();

			activeJobs++;
		}

		job();

		{
			std::lock_guard<std::mutex> lock(queueMutex);
			activeJobs--;
		}

		finishedCV.notify_all();
	}
}