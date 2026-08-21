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
		size_t start;
		size_t index;
		std::unique_ptr<BufferedByteReader> reader;
	};

	// isolated worker task to optimize cache usage and eliminate synchronization locks
	ParsedTrack ParseTrackData(const RawTrackChunk& chunk);
	uint32_t ReadVLQ(BufferedByteReader* reader);
};