#pragma once

#include "TrackEvent.h"
#include <string>

struct NoteEvent : public TrackEvent
{
	uint8_t note;
	uint16_t gate;
	uint8_t vel;
	uint16_t y;
	uint16_t h;

	NoteEvent(uint16_t track, uint8_t channel, long tick, uint8_t note, uint16_t gate, uint8_t vel)
		: TrackEvent(track, channel, tick)
	{
		this->note = note;
		this->gate = gate;
		this->vel = vel;
	}

	const std::string ToString()
	{
		return "Note(" + std::to_string(note) + ", " + std::to_string(gate) + ", " + std::to_string(vel) + ")";
	}
};