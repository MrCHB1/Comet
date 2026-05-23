#pragma once
#include "ADiagnosis.h"
#include <memory>
#include <array>
#include <vector>
#include "../SavedInputStream.h"
#include "DiagnosisField.h"

class BlackMIDIDiagnosis;

class Track
{
public:
	Track() = default;
	Track(size_t trackPosStart, size_t trackPosEnd)
		: trackPosStart(trackPosStart), trackPosEnd(trackPosEnd), noteons(16, std::vector<int>(128)) { }

	void ReadEvent(BlackMIDIDiagnosis& bmd);
	void NoteOn(BlackMIDIDiagnosis& bmd, uint8_t ch, int note);
	void NoteOff(BlackMIDIDiagnosis& bmd, uint8_t ch, int note);

	std::shared_ptr<ProgressInputStream> is;
	size_t trackPosStart;
	size_t trackPosEnd;
	long tickNext = 0;
	int lastMeta = 0;
private:
	std::vector<std::vector<int>> noteons;
};

class OverlapGraph
{
public:
	long section;
	long total;
	std::shared_ptr<std::vector<long>> list;
	std::shared_ptr<LongList> field;

	OverlapGraph() : field(std::make_shared<LongList>()), section(0), total(0), list(std::make_shared<std::vector<long>>())
	{
		field->SetValue(list);
	}

	void Increment()
	{
		section++;
		total++;
	}

	void AddSection()
	{
		list->push_back(section);
		section = 0;
	}
};

class BlackMIDIDiagnosis : public ADiagnosis
{
public:
	BlackMIDIDiagnosis(MIDIApp* app, const char* filePath);
	void Run() override;
	void Stop() override
	{
		isRunning = false;
	}
	double GetProgress() override;
private:
	friend class Track;
	friend class OverlapGraph;

	std::shared_ptr<SavedInputStream> CreateStream();
	void ReadTrack(int idx);
	void RunSequence();

	uint16_t resolution;
	std::shared_ptr<SavedInputStream> pis;
	std::vector<Track> tracks;
	long tick = -1L;
	double time;
	double bpm;
	std::vector<std::vector<int>> noteonsGlobal;
	std::vector<long> nps;
	long npsMax;
	long npsMaxInSection;
	std::unique_ptr<OverlapGraph> overlap;
	std::unique_ptr<OverlapGraph> overlapStream;
	std::shared_ptr<std::vector<long>> npsList;
	Long* fNps;
	std::shared_ptr<LongList> fNpsGraph;
	Long* fOverlap;

	std::ifstream fileStream;
	std::string filePath;
};