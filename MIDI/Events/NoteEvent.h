#pragma once

#include <string>
#include <vector>
#include <cstdint>

/*struct NoteEvent : public TrackEvent
{
	uint32_t gate;
	uint8_t note;
	uint8_t vel;

	NoteEvent(uint16_t track, uint8_t channel, long tick, uint8_t note, uint16_t gate, uint8_t vel)
		: TrackEvent(track, channel, tick)
	{
		this->note = note;
		this->gate = gate;
		this->vel = vel;
	}

	std::string ToString() const
	{
		return "Note(" + std::to_string(note) + ", " + std::to_string(gate) + ", " + std::to_string(vel) + ")";
	}
};*/

// Changed from AoS to SoA for cache optimizations. This is generally better for rendering
struct NoteSequence
{
	std::vector<uint16_t> track;
	std::vector<uint8_t> channel;
	std::vector<uint32_t> tick;
	std::vector<uint32_t> gate;
	std::vector<uint8_t> note;
	std::vector<uint8_t> vel;

	void Emplace(uint16_t trk, uint8_t ch, uint32_t tk, uint8_t nt, uint32_t gt, uint8_t vl)
	{
		track.push_back(trk);
		channel.push_back(ch);
		tick.push_back(tk);
		gate.push_back(gt);
		note.push_back(nt);
		vel.push_back(vl);
	}

	void Reserve(size_t cap)
	{
		track.reserve(cap);
		channel.reserve(cap);
		tick.reserve(cap);
		gate.reserve(cap);
		note.reserve(cap);
		vel.reserve(cap);
	}

	size_t Size() const { return tick.size(); }
	bool Empty() const { return tick.empty(); }

	void Clear()
	{
		track.clear();
		channel.clear();
		tick.clear();
		gate.clear();
		note.clear();
		vel.clear();
	}
};