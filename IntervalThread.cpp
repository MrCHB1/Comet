#include "IntervalThread.h"

IntervalThread::IntervalThread(long intervalMs, std::function<void()> func)
	: intervalMs(intervalMs), func(func) { }

void IntervalThread::Start()
{
	if (running) return;

	shouldStop.store(false);
	running.store(true);

	worker = std::thread([this]()
	{
		while (!shouldStop.load())
		{
			this->func();
			std::this_thread::sleep_for(std::chrono::milliseconds(this->intervalMs));
		}
		running.store(false);
	});
}

void IntervalThread::Stop()
{
	shouldStop.store(true);
	if (worker.joinable())
	{
		if (worker.get_id() == std::this_thread::get_id()) return;
		worker.join();
	}
}