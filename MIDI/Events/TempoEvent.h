#pragma once

#include "MIDIEvent.h"

struct TempoEvent : public MIDIEvent
{
	double bpm;

	TempoEvent(long tick, double bpm) : MIDIEvent(tick)
	{
		this->bpm = bpm;
	}
};