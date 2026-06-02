#include "BasicDiagnosis.h"
#include "../AbstractMIDILoader.h"
#include <array>
#include <algorithm>
#include <iostream>

void BasicDiagnosis::Run()
{
	isRunning = true;
	long l = pis->GetSize();
	fFileSize->SetValue(l);
	trackSizes = std::vector<long>(1);
	
	seq = std::make_unique<MIDISequence>(nullptr);
	try
	{
		std::shared_ptr<std::ifstream> stream = pis->GetStream();

		std::array<uint8_t, 4> header;
		uint8_t* hdrBytes = header.data();
		pis->Read(hdrBytes, 4);
		if (header[0] != 'M' || header[1] != 'T' || header[2] != 'h' || header[3] != 'd')
		{
			throw std::exception("Invalid MIDI Header");
		}
		// header size
		pis->Read(hdrBytes, 4);
		int headerSize = AbstractMIDILoader::ToInt(hdrBytes);
		long posHeaderEnd = pis->GetPosition() + headerSize;
		
		std::array<uint8_t, 2> headerData;
		uint8_t* dataBytes = headerData.data();
		// format
		pis->Read(dataBytes, 2);
		uint16_t format = AbstractMIDILoader::ToShort(dataBytes);
		if (format != 1 && format != 2)
		{
			throw std::exception(("Unsupported MIDI Format: " + std::to_string(format)).c_str());
		}
		// tracks
		pis->Read(dataBytes, 2);
		uint16_t tracks = AbstractMIDILoader::ToShort(dataBytes);
		// PPQ
		pis->Read(dataBytes, 2);
		uint16_t resolution = AbstractMIDILoader::ToShort(dataBytes);
		if (resolution < 480)
		{
			// AddWarning("Low Resolution", { resolution });
		}
		seq->resolution = resolution;
		if (pis->GetPosition() < posHeaderEnd)
		{
			pis->Seek(posHeaderEnd - pis->GetPosition(), SEEK_CUR);
		}
		fTracks->SetValue(static_cast<long long>(tracks));
		fResolution->SetValue(static_cast<long long>(resolution));
		fNotes->SetValue(0L);
		for (int i = 0; isRunning && i < tracks; i++)
		{
			ReadTrack(i);
		}
		if (pis->GetPosition() < pis->GetSize())
		{
			// AddWarning("Extra data in MIDI", 00000);
		}
		std::sort(seq->tempos.begin(), seq->tempos.end(), [](auto a, auto b) { return a.tick > b.tick; });
		if (seq->tempos.empty())
		{
			fTempo->SetValue(120, 120);
		}
		else if (seq->tempos.size() == 1)
		{
			double bpm = ((TempoEvent*)(&seq->tempos[0]))->bpm;
			fTempo->SetValue(bpm, bpm);
		}
		else
		{
			double minBpm = 0;
			double maxBpm = 60000000.0;
			for (TempoEvent& t : seq->tempos)
			{
				double bpm = t.bpm;
				if (minBpm <= bpm) minBpm = bpm;
				if (maxBpm >= bpm) maxBpm = bpm;
			}
			fTempo->SetValue(minBpm, maxBpm);
		}

		for (TempoEvent& t : seq->tempos)
		{
			if (!std::isfinite(t.bpm))
				throw std::exception("Infinite Tempo Event");
		}
		// TODO: Duration
		for (int i = 0; i < trackSizes.size(); i++)
		{
			memory += trackSizes[i];
		}
		fMemory->SetValue(memory);
	}
	catch (std::exception e)
	{
		std::cout << "Basic Diagnosis Failed. " << e.what() << std::endl;
	}

	tracksHuge.clear();
	if (pis) pis->Close();
	isRunning = false;
}

void BasicDiagnosis::ReadTrack(int idx)
{
	std::array<uint8_t, 4> header;
	uint8_t* hdrBytes = header.data();
	pis->Read(hdrBytes, 4);
	if (header[0] != 'M' || header[1] != 'T' || header[2] != 'r' || header[3] != 'k')
	{
		int wrongHeader = AbstractMIDILoader::ToInt(hdrBytes);

		std::stringstream string;
		string << "0x" << std::setfill('0') << std::setw(sizeof(int) * 2) << std::hex << wrongHeader;
		
		throw std::exception(("Invalid Track Header. Expected 4d54726b but got " + string.str()).c_str());
	}

	for (auto& note : noteons)
	{
		for (auto& key : note)
		{
			key = 0;
		}
	}

	pis->Read(hdrBytes, 4);
	long trackLength = static_cast<long>(AbstractMIDILoader::ToInt(hdrBytes));
	if (trackLength > 0xFFFFFFFF)
		tracksHuge.push_back(idx + 1);
	long trackSizeSmallest = 0x7FFFFFFFFFFFFFFF;
	int trackSizeSmallestIdx = 0;
	for (int j = 0; j < trackSizes.size(); j++)
	{
		if (trackSizes[j] == 0L)
		{
			trackSizeSmallestIdx = j;
			break;
		}
		if (trackSizeSmallest < trackSizes[j])
		{
			trackSizeSmallest = trackSizes[j];
			trackSizeSmallestIdx = j;
		}
	}
	trackSizes[trackSizeSmallestIdx] = trackLength;
	long posTrackStart = pis->GetPosition();
	long posTrackEnd = pis->GetPosition() + trackLength;
	reachedEoT = false;
	length = 0L;
	while (pis->GetPosition() < posTrackEnd && !reachedEoT)
		ReadEvent();
	if (pis->GetPosition() != posTrackEnd)
	{
		throw std::exception("Track size mismatch");
	}
	if (length > seq->length)
		seq->SetLength(length);
}

void BasicDiagnosis::ReadEvent()
{
	length += AbstractMIDILoader::ReadVariableLengthValue(pis.get());
	int meta = pis->ReadByte() & 0xFF;
	if ((meta & 0x80) == 0)
	{
		meta = lastMeta;
		pis->Seek(-1, std::ios_base::cur);
	}
	lastMeta = meta;
	if (meta == 0xFF)
	{
		uint8_t type = pis->ReadByte() & 0xFF;
		int len = (int)AbstractMIDILoader::ReadVariableLengthValue(pis.get());
		if (type == 47)
		{
			reachedEoT = true;
		}
		else if (type == 81)
		{
			std::array<uint8_t, 3> tempoData;
			uint8_t* tempoBytes = tempoData.data();
			pis->Read(tempoBytes, 3);
			long bpm = 0l;
			for (int i = 0; i < 3; i++)
			{
				bpm = (bpm << 8) | (static_cast<long>(tempoData[i]) & 0xFF);
			}
			seq->tempos.emplace_back(length, 6.0e7 / bpm);
		}
		else
		{
			pis->Seek(len, std::ios::cur);
		}
	}
	else if (meta >= 128 && meta <= 239)
	{
		int data1 = ReadDataByte();
		int data2 = 0;
		uint8_t ch = (uint8_t)(meta & 0xF);
		if (meta < 192 || meta > 223)
			data2 = ReadDataByte();
		if (meta <= 143)
			NoteOff(ch, data1);
		else if (meta <= 159)
		{
			if (data2 == 0)
			{
				NoteOff(ch, data1);
			}
			else if (data1 < (noteons[ch]).size())
			{
				noteons[ch][data1] = noteons[ch][data1] + 1;
			}
		}
		else if (meta >= 176 && meta <= 191 && data1 == 126 && data2 == 0)
		{
			if ((pis->ReadByte() & 0xFF) != 4) pis->Seek(-1, std::ios::cur);
		}
	}
	else if (meta == 240 || meta == 247)
	{
		pis->Seek(AbstractMIDILoader::ReadVariableLengthValue(pis.get()), std::ios::cur);
	}
}

int BasicDiagnosis::ReadDataByte()
{
	int b = pis->ReadByte() & 0xFF;
	if (!foundUnexpectedStatusByte && (b & 0x80) != 0)
	{
		foundUnexpectedStatusByte = true;
		// AddWarning(...);
	}
	return b;
}

void BasicDiagnosis::NoteOff(uint8_t ch, int note)
{
	if (note < (noteons[ch]).size() && noteons[ch][note] > 0)
	{
		noteons[ch][note] = noteons[ch][note] - 1;
		notes++;
		memory += 43L;
		fNotes->SetValue(notes);
		fMemory->SetValue(memory);
	}
}