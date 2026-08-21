#pragma once

#include <memory>
#include "MIDI/MIDIDefs.h"
#include "Render/RenderView.h"
#include <vector>
#include <array>

struct NoteCullRange
{
	float start = 1.0;
	float end = 1.0;
};

class MIDISequence;

// handles note culling
class NoteCuller
{
public:
	NoteCuller() {}
	void CalculateCull(MIDISequence* seq, RenderView* renderView, long midiTime, double timeNorm);
	void ResetCull()
	{
		for (size_t& id : startRenderIDs) { id = 0; }
		MarkDirty();
	}

	void MarkDirty() { cullDirty = true; }

	const size_t GetNotesPassed() const { return notesPassed; }
	const size_t GetKeyCullStart(uint8_t key) const { return startRenderIDs[key]; }

	void SetCullRangeStart(float start)
	{
		if (start < 1.0) start = 1.0;
		cullRange.start = start;
		MarkDirty();
	}

	void SetCullRangeEnd(float end)
	{
		if (end < 1.0) end = 1.0;
		cullRange.end = end;
	}

	NoteCullRange cullRange;
private:
	std::array<size_t, MIDI_KEYS> startRenderIDs{};

	size_t notesPassed = 0;

	bool cullDirty = true;
	long lastTime = 0;
};