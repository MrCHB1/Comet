#pragma once
#include <algorithm>
#include <atomic>
#include <execution>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

namespace Utils
{
	template <typename T, typename F>
	void ParForEach(T first, T last, F&& func)
	{
		unsigned int hardwareThreads = std::thread::hardware_concurrency();
		if (hardwareThreads == 0) hardwareThreads = 16;

		ptrdiff_t size = last - first;
		if (size <= 0) return;

		// fast path: just do a regular for-loop if number of threads is greater than the size
		if (hardwareThreads >= size)
		{
			for (auto it = first; it != last; it++)
			{
				func(*it);
			}
			return;
		}

		unsigned int threadCount = std::min<size_t>(hardwareThreads, size);
		std::vector<std::thread> workers;
		workers.reserve(threadCount);
		std::atomic<size_t> currIdx{ 0 };

		std::exception_ptr exception;
		std::mutex exceptionMutex;
		std::atomic<bool> failed{ false };

		for (unsigned int workerIdx = 0; workerIdx < threadCount; workerIdx++)
		{
			workers.emplace_back(
				[&currIdx, &first, &func, &size,
				 &failed, &exceptionMutex, &exception]()
				{
					try
					{
						size_t wIdx = 0;
						while (!failed.load(std::memory_order_relaxed) && 
							(wIdx = currIdx.fetch_add(1, std::memory_order_relaxed)) < size)
						{
							func(*(first + wIdx));
						}
					}
					catch (...)
					{
						failed.store(true, std::memory_order_relaxed);
						std::lock_guard lock(exceptionMutex);
						if (!exception) exception = std::current_exception();
					}
				});
		}

		for (auto& worker : workers)
		{
			if (worker.joinable()) worker.join();
		}

		if (exception) std::rethrow_exception(exception);
	}
}