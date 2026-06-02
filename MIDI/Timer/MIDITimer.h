#pragma once

#include <chrono>

class MIDITimer
{
public:
	MIDITimer() = default;

	void Start(double startTime = 0.0);
	void Stop();

	void Pause();
	void Resume();
	void TogglePause();

	void NavigateTo(double timeSeconds);

	bool IsRunning() const { return running; }
	bool IsPaused() const { return paused; }

	double Elapsed() const;
	double GetDeltaTime();
private:
	using clock = std::chrono::steady_clock;

	clock::time_point startPoint
		= clock::now();
	double baseTime = 0.0; // accumulated time before the current run segment
	double lastTime = 0.0; // last value returned by Elapsed/GetDeltaTime

	bool running = false;
	bool paused = true;
};