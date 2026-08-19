#pragma once

#include "AbstractMIDILoader.h"
#include <string>
#include <memory>
#include <vector>
#include <array>
#include <future>
#include "MIDISequence.h"

class BufferedByteReader;

class MultithreadedMIDILoader : public AbstractMIDILoader
{
public:
	MultithreadedMIDILoader(const char* file);
	MultithreadedMIDILoader(std::shared_ptr<InputStream> is);
	~MultithreadedMIDILoader()
	{
		Stop();
		seq.reset();
	}

	std::shared_ptr<MIDISequence> Load(bool timeBasedLoading = false) override;

	void Stop() override
	{
		AbstractMIDILoader::Stop();
		running = false;
	}

private:
	std::string file;
	std::shared_ptr<MIDISequence> seq;
	std::shared_ptr<InputStream> is;
	std::shared_ptr<ProgressInputStream> pis;
	std::mutex fileMutex;

	std::atomic<bool> running{ false };
	std::atomic<size_t> tracksProcessed{ 0 };

	// context structure holding raw track bytes before processing
	struct RawTrackChunk
	{
		size_t index;
		std::unique_ptr<BufferedByteReader> reader;
	};

	// output structure for an isolated track parsing session
	struct ParsedTrackResult
	{
		size_t trackIndex;
		size_t numNotes = 0;
		NoteSequence notes;
		std::vector<MIDIMessageEvent> events;
		std::vector<TempoEvent> tempos;
		std::vector<TimeSignatureEvent> timeSignatures;
		bool mixedChannelsInTrack = false;
		long length = 0;
	};

	// isolated worker task to optimize cache usage and eliminate synchronization locks
	ParsedTrackResult ParseTrackData(const RawTrackChunk& chunk);
	uint32_t ReadVLQ(BufferedByteReader* reader);
};