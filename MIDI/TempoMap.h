#pragma once

#include "MIDISequence.h"
#include <memory>
#include <vector>

struct TempoPoint
{
	long tick;
	double tempo;
	TempoPoint(long tick, double tempo, double secsAtTick) : 
		tick(tick), tempo(tempo), secsAtTick(secsAtTick) { }

public:
	double GetSecsAtTick() const { return secsAtTick; }

private:
	double secsAtTick;
};

class TempoMap
{
public:
	TempoMap() = default;
	void RebuildTempoMap(MIDISequence* seq);
	double TicksToSecsFromMap(uint16_t ppq, long tick);
	double GetBPMAtTick(long tick);
	long SecsToTicksFromMap(uint16_t ppq, double secs);
private:
	std::vector<TempoPoint> tempoMap{};
};