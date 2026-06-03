#pragma once
#include "ADiagnosis.h"
#include "../SavedInputStream.h"
#include "../MIDISequence.h"

class BasicDiagnosis : public ADiagnosis
{
public:
	BasicDiagnosis(MIDIApp* app, const char* filePath) :
		ADiagnosis(app, filePath),
		noteons(16, std::vector<int>(128)),
		tracksHuge(std::vector<int>(1)),
		fFileSize(CreateField<FileSize>("File Size")),
		fResolution(CreateField<Long>("Resolution")),
		fTracks(CreateField<Long>("Tracks")),
		fNotes(CreateField<Long>("Notes")),
		fMemory(CreateField<FileSize>("Memory to Open")),
		fTempo(CreateField<Double>("Tempo")),
		lastMeta(0),
		reachedEoT(false),
		foundUnexpectedStatusByte(false),
		notes(0),
		memory(0),
		length(0)
	{
		std::shared_ptr<std::ifstream> fileStream = std::make_shared<std::ifstream>(filePath, std::ios::binary);
		fileStream->seekg(0, std::ios::end);
		long fileLength = fileStream->tellg();
		fileStream->seekg(0, std::ios::beg);

		pis = std::make_shared<ProgressInputStream>(fileStream, fileLength);
	}

	void Run() override;
	void Stop() override
	{
		isRunning = false;

	}
	double GetProgress() override
	{
		return (pis == nullptr) ? 0 : pis->GetProgress();
	}
private:
	std::shared_ptr<ProgressInputStream> pis;
	int lastMeta;
	std::vector<std::vector<int>> noteons;
	bool reachedEoT = false;
	bool foundUnexpectedStatusByte = false;
	long notes;
	long long memory;
	std::vector<int> tracksHuge;
	std::vector<long> trackSizes;
	long length;
	std::unique_ptr<MIDISequence> seq;
	FileSize* fFileSize;
	Long* fResolution;
	Long* fTracks;
	Long* fNotes;
	FileSize* fMemory;
	// Duration fDuration;
	Double* fTempo;

	void ReadTrack(int idx);
	void ReadEvent();
	int ReadDataByte();
	void NoteOff(uint8_t ch, int note);
};