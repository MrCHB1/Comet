#pragma once

#include <vector>
#include "MIDITrack.h"
#include "Events/TempoEvent.h"
#include "Events/NoteEvent.h"
#include "Events/TimeSignatureEvent.h"
#include <fstream>
#include <memory>
#include "MIDIDefs.h"
#include <atomic>

#define NOTE_BLOCK_SIZE 1024

class TempoMap;

class MIDISequence
{
public:
	const char* name;
	uint16_t resolution;
	uint16_t trackCount;
	std::vector<MIDITrack> tracks{};
	std::vector<TempoEvent> tempos{};
	std::vector<TimeSignatureEvent> timeSignatures{};
	std::array<NoteSequence, MIDI_KEYS> mergedNotes{};
	std::array<std::vector<NoteBlock>, MIDI_KEYS> noteBlocks{};
	std::vector<MIDIMessageEvent> mergedEvents{};
	std::unique_ptr<TempoMap> tempoMap;
	size_t noteTrackCount;
	long long notes = 0;
	long length = 0;
	uint32_t longestNoteLength = 0;
	bool timeBased = false;
	MIDISequence();
	MIDISequence(const char* name);
	~MIDISequence();

	TempoMap* GetTempoMap();

	long GetNotes()
	{
		return this->notes;
	}
	long GetLength()
	{
		return this->length;
	}

	void SetLength(long length)
	{
		this->length = length;
	}

	double CalcLengthMilliseconds();

	static void Read(std::ifstream* is, size_t size, uint8_t* buf)
	{
		is->read(reinterpret_cast<char*>(buf), size);
	}
	static long ReadVariableLengthValue(std::ifstream* is)
	{
		uint8_t b;
		long value = 0L;
		do
		{
			is->read(reinterpret_cast<char*>(&b), 1);
			value = (value << 7L) | (b & 0x7F);
		} while ((b & 0x80) != 0);
		return value;
	}
	static uint32_t ToInt(uint8_t* bytes)
	{
		uint32_t res = 0;
		for (int i = 0; i < 4; i++)
		{
			res = (res << 8) | static_cast<uint32_t>(bytes[i]);
		}
		return res;
	}
	static uint64_t ToLong(uint8_t* bytes)
	{
		uint64_t res = 0;
		for (int i = 0; i < 8; i++)
		{
			res = (res << 8) | static_cast<uint64_t>(bytes[i]);
		}
		return res;
	}

	// Call this ONLY when loading a new midi.
	[[nodiscard]] static uint64_t NextSequenceID() noexcept
	{
		static std::atomic<uint64_t> sequenceID = 0;
		return sequenceID.fetch_add(1, std::memory_order_relaxed);
	}

	void SetSequenceID(uint64_t id) noexcept { sequenceID = id; }
	[[nodiscard]] uint64_t GetSequenceID() const noexcept { return sequenceID; }
private:
	uint64_t sequenceID = 0;
};