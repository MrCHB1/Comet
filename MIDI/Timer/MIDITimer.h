#pragma once

#include <chrono>

class MIDITimer
{
public:
	MIDITimer() = default;

	void Start();
	double Elapsed();
	double GetDeltaTime();
private:
	double lastTime;
	std::chrono::steady_clock::time_point time
		= std::chrono::steady_clock::now();
};