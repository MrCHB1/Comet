#pragma once

#define MIDI_KEYS 128
#define TIME_BASED_MULTIPLIER 100000

static inline bool IsSharp(size_t key)
{
	size_t k12 = key % 12;
	return k12 == 1 || k12 == 3 || k12 == 6 || k12 == 8 || k12 == 10;
}

static inline size_t GetNumWhiteKeys(size_t start, size_t end)
{
	size_t count = 0;
	for (size_t i = start; i < end; i++)
	{
		if (IsSharp(i)) continue;
		count++;
	}
	return count;
}