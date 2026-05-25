#pragma once

#include <vector>
#include "MIDITrack.h"
#include "Events/TempoEvent.h"
#include "Events/NoteEvent.h"
#include <fstream>
#include <memory>

class TempoMap;

class MIDISequence
{
public:
	const char* name;
	uint16_t resolution;
	std::vector<MIDITrack> tracks;
	std::vector<TempoEvent> tempos;
	std::vector<std::vector<NoteEvent>> mergedNotes{};
	std::shared_ptr<TempoMap> tempoMap = nullptr;
	size_t noteTrackCount;
	long notes = 0;
	long length = 0;

	MIDISequence() : MIDISequence("Unnamed") {}
	MIDISequence(const char* name)
		: name(name), tracks({}), tempos({}) {}
	~MIDISequence()
	{
		tracks.clear();
		tempos.clear();
		mergedNotes.clear();
		if (tempoMap) tempoMap.reset();
	}

	TempoMap* GetTempoMap()
	{
		return tempoMap.get();
	}
	
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
};