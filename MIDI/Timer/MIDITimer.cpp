#include "MIDITimer.h"
#include <chrono>

void MIDITimer::Start(double startTime)
{
	running = true;
	paused = false;
	baseTime = startTime;
	lastTime = startTime;
	startPoint = clock::now();
}

void MIDITimer::Stop()
{
	running = false;
	paused = false;
	baseTime = 0.0;
	lastTime = 0.0;
}

void MIDITimer::Pause()
{
	if (!running || paused) return;
	baseTime = Elapsed();
	paused = true;
	lastTime = baseTime;
}

void MIDITimer::Resume()
{
	if (!running || !paused) return;
	paused = false;
	startPoint = clock::now();
	lastTime = baseTime;
}

void MIDITimer::TogglePause()
{
	if (!running) return;

	if (paused) Resume();
	else Pause();
}

void MIDITimer::NavigateTo(double timeSeconds)
{
	baseTime = timeSeconds;
	lastTime = timeSeconds;

	if (running && !paused)
	{
		startPoint = clock::now();
	}
}

double MIDITimer::Elapsed() const
{
	if (!running || paused) return baseTime;

	auto now = clock::now();
	return baseTime + std::chrono::duration<double>(now - startPoint).count();
}

double MIDITimer::GetDeltaTime()
{
	double now = Elapsed();
	double delta = now - lastTime;
	lastTime = now;

	if (!running || paused) return 0.0;

	return delta;
}