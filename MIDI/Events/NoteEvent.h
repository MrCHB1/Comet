#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "RLEvec.h"

#define NOTE_VELOCITY_MASK 0x7F
#define NOTE_VELOCITY_BIT_IDX 8
#define NOTE_CHANNEL_MASK 0xF
#define NOTE_CHANNEL_BIT_IDX 15
#define NOTE_TRACK_MASK 0x1FFF
#define NOTE_TRACK_BIT_IDX 19

// Changed from AoS to SoA for cache optimizations. This is generally better for rendering
struct NoteSequence
{
	std::vector<uint32_t> tick;
	std::vector<uint32_t> gate;
	std::vector<uint32_t> packed;

	void Emplace(uint16_t trk, uint8_t ch, uint32_t tk, uint8_t nt, uint32_t gt, uint8_t vl)
	{
		tick.push_back(tk);
		gate.push_back(gt);

		uint32_t data = (uint32_t)nt | // key
			((uint32_t)(vl & NOTE_VELOCITY_MASK) << NOTE_VELOCITY_BIT_IDX) | // velocity
			((uint32_t)(ch & NOTE_CHANNEL_MASK) << NOTE_CHANNEL_BIT_IDX) | // channel
			((uint32_t)(trk & NOTE_TRACK_MASK) << NOTE_TRACK_BIT_IDX); // tracks can only go from 1 -> 8191 before wrapping. 

		packed.push_back(data);
	}

	void Reserve(size_t cap)
	{
		tick.reserve(cap);
		gate.reserve(cap);
		packed.reserve(cap);
	}

	void Resize(size_t count)
	{
		tick.resize(count);
		gate.resize(count);
		packed.resize(count);
	}

	size_t Size() const { return tick.size(); }
	bool Empty() const { return tick.empty(); }

	void Clear()
	{
		tick.clear();
		gate.clear();
		packed.clear();
	}

	const uint32_t GetTick(size_t index) const
	{
		return tick[index];
	}

	const uint32_t GetEndTick(size_t index) const
	{
		return tick[index] + gate[index];
	}

	uint8_t GetKey(size_t index) const
	{
		return (uint8_t)(packed[index] & 0xFF);
	}

	uint8_t GetVelocity(size_t index) const
	{
		return (uint8_t)(packed[index] >> 8) & 0x7F;
	}

	uint8_t GetChannel(size_t index) const
	{
		return (uint8_t)(packed[index] >> 15) & 0xF;
	}

	uint16_t GetTrack(size_t index) const
	{
		return (uint16_t)(packed[index] >> 19) & 0x1FFF;
	}
};

// Keeps track of 1,024 notes and identifies the minimum and maximum bounds of those 1,024 notes
struct NoteBlock
{
	uint32_t minBound;
	uint32_t maxBound;

	NoteBlock(uint32_t min, uint32_t max) : minBound(min), maxBound(max) {}
};