#pragma once

#include "MIDI/MIDISequence.h"
#include "MIDI/Timer/MIDITimer.h"
#include "MIDIOut.h"
#include <memory>
#include <atomic>
#include <mutex>
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

class AudioThread
{
public:
	AudioThread(std::shared_ptr<MIDIOut> midiOut) : midiOut(midiOut) { }
	~AudioThread()
	{
		Stop();
	}
	void Start(std::shared_ptr<MIDISequence> seq, std::shared_ptr<MIDITimer> timer);
	void Stop()
	{
		stopFlag = true;
		if (audioThread.joinable())
		{
			audioThread.join();
		}
		threadWorking = false;
	}
	void Reset()
	{
		if (!threadWorking) return;
		Stop();
	}

	void MuteAudio()
	{
		audioMuted = true;
	}

	void UnmuteAudio()
	{
		audioMuted = false;
	}
private:
	std::shared_ptr<MIDIOut> midiOut;
	std::atomic_bool stopFlag = false;
	bool threadWorking = false;
	std::atomic_bool audioMuted = false;
	std::thread audioThread;

	void ResetEvents();
};