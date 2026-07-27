#pragma once

#include <cstdint>
#include <vector>

#include "Events/NoteEvent.h"
#include "Events/MIDIMessageEvent.h"

struct MIDITrack
{
	uint32_t color;
	NoteSequence notes;
	std::vector<MIDIMessageEvent> messages{};
};