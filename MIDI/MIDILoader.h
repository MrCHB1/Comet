#pragma once
#include "AbstractMIDILoader.h"
#include <string>
#include <memory>
#include "MIDISequence.h"
#include <array>
#include <mutex>
#include <unordered_map>
#include <stack>

class MIDILoader : public AbstractMIDILoader
{
public:
	MIDILoader(const char* file);
	MIDILoader(std::shared_ptr<InputStream> is);
	~MIDILoader()
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
	std::mutex loaderMtx;
	double prog = 0;

	bool running = false;
	size_t currNoteId = 0;

	ParsedTrack LoadTrack(std::shared_ptr<InputStream> is, int track);
};