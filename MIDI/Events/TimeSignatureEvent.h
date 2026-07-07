#pragma once

#include "MIDIEvent.h"

// Used mainly for drawing measure/bar lines with renderers such as MIDITrail
struct TimeSignatureEvent : public MIDIEvent
{
	uint8_t numerator;
	uint8_t denominator;

	TimeSignatureEvent(long tick, uint8_t nn, uint8_t dd) : MIDIEvent(tick), numerator(nn), denominator(dd)
	{

	}
};