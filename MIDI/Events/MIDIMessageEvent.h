#pragma once

#include "MIDIEvent.h"
#include <vector>
#include <cstdint>

struct MIDIMessageEvent : public MIDIEvent
{
	uint32_t message;
	MIDIMessageEvent(long tick, uint32_t message) : MIDIEvent(tick)
	{
		this->message = message;
	}
};