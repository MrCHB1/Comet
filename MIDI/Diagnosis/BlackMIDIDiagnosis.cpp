#include "BlackMIDIDiagnosis.h"
#include "../AbstractMIDILoader.h"
#include <iostream>
#include "../../IO/BufferedByteReader.h"

BlackMIDIDiagnosis::BlackMIDIDiagnosis(MIDIApp* app, const char* filePath)
	: ADiagnosis(app, filePath), filePath(filePath),
	noteonsGlobal(16, std::vector<int>(128)),
	bpm(0),
	npsMax(0),
	npsMaxInSection(0),
	resolution(0),
	time(0)
{
	nps = std::vector<long>(1000);
	npsList = std::make_shared<std::vector<long>>(1000);

	overlap = std::make_unique<OverlapGraph>();
	overlapStream = std::make_unique<OverlapGraph>();

	fNps = CreateField<Long>("Moment Max NPS");
	fNpsGraph = std::make_shared<LongList>();
	fNpsGraph->SetValue(npsList);
	fNps->additionalFields.push_back(fNpsGraph);

	fOverlap = CreateField<Long>("Overlapping Notes");
	fOverlap->additionalFields.push_back(overlap->field);
	fOverlap->additionalFields.push_back(overlapStream->field);
}

void BlackMIDIDiagnosis::Run()
{
	isRunning = true;
	tick = -1L;
	fNps->SetValue(0L);
	fOverlap->SetValue(0L);
	try
	{
		pis = CreateStream();
		pis->Seek(4L, std::ios::cur);
		std::array<uint8_t, 4> intBytes{};
		std::array<uint8_t, 2> shortBytes{};
		uint8_t* ibData = intBytes.data();
		uint8_t* sbData = shortBytes.data();

		// read header size
		pis->Read(ibData, 4);
		int headerSize = AbstractMIDILoader::ToInt(ibData);
		long posHeaderEnd = pis->GetPosition() + headerSize;
		pis->Seek(2L, std::ios::cur);
		
		// read tracks
		pis->Read(sbData, 2);
		uint16_t tracks = AbstractMIDILoader::ToShort(sbData);
		pis->Read(sbData, 2);
		resolution = AbstractMIDILoader::ToShort(sbData);
		if (pis->GetPosition() < posHeaderEnd)
			pis->Seek(posHeaderEnd - pis->GetPosition(), std::ios::cur);
		this->tracks = std::vector<Track>(tracks);
		for (int i = 0; isRunning && i < tracks; i++)
			ReadTrack(i);
		pis->Close();
		pis.reset();
		RunSequence();
	}
	catch (const std::runtime_error& e)
	{
		std::cout << "Black MIDI Diagnosis failed." << e.what() << std::endl;
	}

	if (pis) pis->Close();

	if (overlap->total > 0L)
	{
		// AddWarning("Overlap");
	}
	isRunning = false;
}

std::shared_ptr<SavedInputStream> BlackMIDIDiagnosis::CreateStream()
{
	auto fs = std::make_shared<std::ifstream>(filePath.c_str(), std::ios::binary);
	fs->seekg(0, std::ios::end);
	long size = fs->tellg();
	fs->seekg(0, std::ios::beg);
	return std::make_shared<SavedInputStream>(fs, size);
}

void BlackMIDIDiagnosis::ReadTrack(int idx)
{
	std::array<uint8_t, 4> intBytes{};
	uint8_t* ibData = intBytes.data();

	pis->Seek(4L, std::ios::cur);
	pis->Read(ibData, 4);
	int trackLength = AbstractMIDILoader::ToInt(ibData);
	long posDataStart = pis->GetPosition();
	long posDataEnd = pis->GetPosition() + trackLength;
	tracks[idx] = Track(posDataStart, posDataEnd);
	pis->Seek(trackLength, std::ios::cur);
}

void BlackMIDIDiagnosis::RunSequence()
{
	int i;
	bpm = 120;
	time = 0;
	for (auto& n : nps)
	{
		n = 0L;
	}
	npsMax = 0L;
	overlap->total = 0L;
	overlapStream->total = 0L;
	long msNps = 0L, msNpsLast = 0L;
	try
	{
		for (auto& trk : tracks)
		{
			trk.is = CreateStream();
			trk.is->Seek(trk.trackPosStart, std::ios::beg);
			trk.tickNext = AbstractMIDILoader::ReadVariableLengthValue(trk.is.get());
		}

		for (tick = 0; isRunning; tick++)
		{
			if (tick % resolution == 0)
			{
				std::cout << "Time: " << time / 1000 << "s  BPM: " << bpm << "  Tick: " << tick << std::endl;
			}
			if (msNps != msNpsLast)
			{
				for (long& n : nps)
				{
					n = 0L;
				}
			}
			else
			{
				for (long ms1 = msNpsLast + 1; ms1 <= msNps; ms1++)
				{
					nps[(int)(ms1 % nps.size())] = 0L;
				}
			}
			bool hasMoreEvents = false;
			for (int k = 0; k < tracks.size(); k++)
			{
				auto& t = tracks[k];
				if (t.tickNext != -1L)
				{
					hasMoreEvents = true;
					while (tick == t.tickNext)
						t.ReadEvent(*this);
				}
			}
			if (!hasMoreEvents) isRunning = false;
			long npsTotal = 0L;
			for (int k = 0; k < nps.size(); k++)
			{
				npsTotal += nps[k];
			}
			if (npsTotal > npsMax)
			{
				npsMax = npsTotal;
				fNps->SetValue(npsMax);
			}
			if (npsTotal > npsMaxInSection)
			{
				npsMaxInSection = npsTotal;
			}
			if (tick % (resolution / 4) == 0L)
			{
				npsList->push_back(npsMaxInSection);
				npsMaxInSection = 0L;
				overlap->AddSection();
				overlapStream->AddSection();
			}
			time += 60000.0 / bpm / (double)resolution;
			msNpsLast = msNps;
			msNps = (long)time;
		}
	}
	catch (const std::runtime_error& e)
	{
		std::cout << "Failed to run Sequence: " << e.what() << std::endl;
	}

	std::cout << std::endl;
	for (int j = 0; j < tracks.size(); j++)
	{
		auto& t = tracks[j];
		if (t.is)
		{
			try
			{
				t.is->Close();
			}
			catch (const std::runtime_error& e) { }
		}
	}
	npsList->shrink_to_fit();
	std::cout << "Black MIDI diagnosis finished!" << std::endl;
}

double BlackMIDIDiagnosis::GetProgress()
{
	if (tick == -1L) return -1;
	double progTotal = 0;
	for (int i = 0; i < tracks.size(); i++)
	{
		Track* t = &(tracks[i]);
		if (t->is)
		{
			long p = t->is->GetPosition() - t->trackPosStart;
			long l = t->trackPosEnd - t->trackPosStart;
			progTotal += (double)p / (double)l;
		}
	}
	return progTotal / (double)tracks.size();
}

void Track::ReadEvent(BlackMIDIDiagnosis& bmd)
{
	int meta = is->ReadByte() & 0xFF;
	if ((meta & 0x80) == 0)
	{
		meta = lastMeta;
		is->Seek(-1, std::ios::cur);
	}
	lastMeta = meta;
	if (meta == 255)
	{
		int type = is->ReadByte() & 0xFF;
		int len = AbstractMIDILoader::ReadVariableLengthValue(is.get());
		if (type == 81)
		{
			std::array<uint8_t, 3> bpmData{};
			uint8_t* bpmRaw = bpmData.data();
			is->Read(bpmRaw, 3);

			uint32_t bpm = 0;
			for (int i = 0; i < 3; i++)
			{
				bpm = (bpm << 8) | (static_cast<uint32_t>(bpmRaw[i]) & 0xFF);
			}

			bmd.bpm = 6.0e7 / (double)bpm;
		}
		else
		{
			if (type == 47)
			{
				tickNext = -1L;
				return;
			}
			is->Seek(len, std::ios::cur);
		}
	}
	else if (meta >= 128 && meta <= 239)
	{
		int data1 = is->ReadByte() & 0xFF;
		int data2 = 0;
		uint8_t ch = (uint8_t)(meta & 0xF);
		if (meta < 192 || meta > 223)
			data2 = is->ReadByte() & 0xFF;
		if (meta <= 143)
		{
			NoteOff(bmd, ch, data1);
		}
		else if (meta <= 159)
		{
			if (data2 == 0) NoteOff(bmd, ch, data1);
			else
				NoteOn(bmd, ch, data1);
		}
		else if (meta >= 176 && meta <= 191 && data1 == 126 && data2 == 0)
		{
			if ((is->ReadByte() & 0xFF) != 4) is->Seek(-1, std::ios::cur);
		}
	}
	else if (meta == 240 || meta == 247)
	{
		int sysexLen = AbstractMIDILoader::ReadVariableLengthValue(is.get());
		is->Seek(sysexLen, std::ios::cur);
	}

	if (is->GetPosition() < is->GetSize())
	{
		tickNext += AbstractMIDILoader::ReadVariableLengthValue(is.get());
	}
	else
	{
		tickNext = -1L;
	}
}

void Track::NoteOn(BlackMIDIDiagnosis& bmd, uint8_t ch, int note)
{
	if (noteons[ch][note] > 0)
	{
		bmd.overlap->Increment();
		bmd.fOverlap->SetPrimaryValue(bmd.overlap->total);
	}
	noteons[ch][note]++;
	if (bmd.noteonsGlobal[ch][note] > 0)
	{
		bmd.overlapStream->Increment();
		bmd.fOverlap->SetSecondaryValue(bmd.overlapStream->total);
	}
	bmd.noteonsGlobal[ch][note]++;
	bmd.nps[((int)bmd.time % bmd.nps.size())]++;
}

void Track::NoteOff(BlackMIDIDiagnosis& bmd, uint8_t ch, int note)
{
	if (noteons[ch][note] > 0)
	{
		noteons[ch][note]--;
	}
	if (bmd.noteonsGlobal[ch][note] > 0)
		bmd.noteonsGlobal[ch][note]--;
}
