#pragma once
#include <thread>
#include <functional>
#include <atomic>

class IntervalThread
{
public:
	IntervalThread(long intervalMs, std::function<void()> func);
	~IntervalThread()
	{
		Stop();
		if (worker.joinable()) worker.join();
	}
	void Start();
	void Stop();
private:
	std::function<void()> func;
	long intervalMs;
	std::atomic<bool> shouldStop = false;
	std::atomic<bool> running = false;

	std::thread worker;
};