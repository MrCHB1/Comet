#pragma once
#include "AbstractMIDILoader.h"
#include <string>
#include <memory>
#include "MIDISequence.h"
#include <array>
#include <mutex>
#include <unordered_map>
#include <stack>

#include "imgui.h"

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
	void Stop()
	{
		running = false;
	}
private:
	std::string file;
	std::array<std::unique_ptr<MIDITrack>, 16> channels{};
	std::array<uint32_t, 16> colors{};
	// std::unordered_map<uint16_t, std::stack<size_t>> unendedNotes{};
	std::array<std::vector<size_t>, 2048> unendedNotes{};
	std::vector<NoteEvent> noteons{};
	std::shared_ptr<MIDISequence> seq;
	std::shared_ptr<InputStream> is;
	std::shared_ptr<ProgressInputStream> pis;
	std::mutex loaderMtx;
	double prog = 0;

	bool running = false;
	size_t currNoteId = 0;

	void LoadTrack(std::shared_ptr<InputStream> is, int track);
	inline void NoteOff(uint8_t ch, uint8_t data1, long tick);
	inline void ClearUnendedNotes()
	{
		for (auto& note : unendedNotes)
		{
			note.clear();
		}
	}
	inline MIDITrack* GetChannelTrack(uint8_t ch);
};