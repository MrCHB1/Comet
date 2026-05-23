#pragma once

#include "MIDIEvent.h"

struct TrackEvent : public MIDIEvent
{
	uint16_t track;
	uint8_t channel;
	TrackEvent(uint16_t track, uint8_t channel, long tick) : MIDIEvent(tick)
	{
		this->track = track;
		this->channel = channel;
	}
};