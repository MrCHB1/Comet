#include "MIDITimer.h"
#include <chrono>

void MIDITimer::Start()
{
	time = std::chrono::steady_clock::now();
	lastTime = Elapsed();
}

double MIDITimer::Elapsed()
{
	auto now = std::chrono::steady_clock::now();

	return std::chrono::duration<double>(
		now - time
	).count();
}

double MIDITimer::GetDeltaTime()
{
	double now = Elapsed();
	double delta = now - lastTime;
	lastTime = now;
	return delta;
}