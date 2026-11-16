#pragma once

#include <cstdint>
#include <array>
#include <atomic>
#include <vector>
#include <future>
#include "MIDI/MIDIDefs.h" // for MIDI_KEYS
#include "Concurrency/ThreadPool.h"
#include "KeyNoteIndex.h"

struct KeyNoteIndexData
{
	uint64_t sourceSequenceID = 0;
	struct KeyJob { uint8_t id; size_t startCap; bool sparse; };

	std::array<KeyJob, MIDI_KEYS> jobs;
	std::array<std::vector<uint32_t>, MIDI_KEYS> keepPerKey; // persistent, never shrinks
	std::vector<std::future<void>> collectFutures;           // reserve(WorkerCount) once
	std::atomic<size_t> nextJob{ 0 };

	std::array<KeyNoteIndex, MIDI_KEYS> keyNoteIndices{};
	ThreadPool threadPool;
};