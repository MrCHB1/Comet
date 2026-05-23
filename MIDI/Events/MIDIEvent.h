#pragma once

#include <cstdint>

struct MIDIEvent
{
	static const uint8_t TEMPO = 81;
	static const uint8_t END_OF_TRACK = 47;
	static const uint8_t UNDEFINED_TEXT = 10;
	long tick;

	MIDIEvent(long tick)
	{
		this->tick = tick;
	}
};