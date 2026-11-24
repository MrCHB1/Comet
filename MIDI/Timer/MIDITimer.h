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
	void ChangeSpeed(double speed);
	
	[[nodiscard]] bool HasNavigatedRecently()
	{
		return std::exchange(hasNavigated, false);
	}

	[[nodiscard]] bool PauseRequestedRecently()
	{
		return std::exchange(pauseRequested, false);
	}

	[[nodiscard]] bool SpeedChanged()
	{
		return std::exchange(speedChanged, false);
	}

	bool IsRunning() const { return running; }
	bool IsPaused() const { return paused; }

	double Elapsed() const;
	double GetPauseTime() const { return baseTime; }
	double GetDeltaTime();

	double GetSpeed() const { return speed; }

	void ClearFlags()
	{
		pauseRequested = false;
		hasNavigated = false;
	}
private:
	using clock = std::chrono::steady_clock;

	clock::time_point startPoint
		= clock::now();
	double baseTime = 0.0; // accumulated time before the current run segment
	double lastTime = 0.0; // last value returned by Elapsed/GetDeltaTime
	double speed = 1.0;

	bool running = false;
	bool paused = true;
	bool pauseRequested = false;
	bool hasNavigated = false;
	bool speedChanged = false;
};