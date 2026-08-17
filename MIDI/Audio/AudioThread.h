#pragma once

#include "../MIDISequence.h"
#include "../Timer/MIDITimer.h"
#include "MIDIOut.h"
#include "MIDIAudio.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>

struct ScheduledEvent
{
	MIDIMessageEvent event;
};

struct CompareTick
{
	bool operator()(const ScheduledEvent& a, const ScheduledEvent& b) const
	{
		return a.event.tick > b.event.tick;
	}
};

class AudioThread : public AudioEngine
{
public:
	AudioThread(std::shared_ptr<MIDIOut> midiOut) : midiOut(midiOut) { }
	~AudioThread()
	{
		Stop();
	}

	std::string GetName() override
	{
		return "Realtime Audio";
	}

	bool IsSupported() override
	{
		return true;
	}

	void Initialize() override
	{

	}

	void Destroy() override
	{
		Stop();
	}

	void Start(std::shared_ptr<MIDISequence> seq, std::shared_ptr<MIDITimer> timer) override;
	void Stop() override
	{
		stopFlag = true;
		if (audioThread.joinable())
		{
			audioThread.join();
		}
		threadWorking = false;
	}
	void Reset() override
	{
		if (!threadWorking) return;
		Stop();
	}

	void Mute() override
	{
		audioMuted = true;
	}

	void Unmute() override
	{
		audioMuted = false;
	}

	bool IsPlaying() override
	{
		return threadWorking;
	}
private:
	std::shared_ptr<MIDIOut> midiOut;
	std::atomic_bool stopFlag = false;
	bool threadWorking = false;
	std::atomic_bool audioMuted = false;
	std::thread audioThread;

	void ResetEvents();
};