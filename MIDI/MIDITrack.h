#pragma once

#include <cstdint>
#include <vector>

#include "Events/NoteEvent.h"
#include "Events/MIDIMessageEvent.h"

struct MIDITrack
{
	uint32_t color;
	std::vector<NoteEvent> notes{};
	std::vector<MIDIMessageEvent> messages{};
};