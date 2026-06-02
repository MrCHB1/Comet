// TODO - also have multithreaded parsing as an option

#include "MIDILoader.h"
#include <filesystem>
#include "IO/BufferedByteReader.h"
#include "Comet.h"
#include <algorithm>
#include "Sequence/SequenceFuncs.h"
#include "TempoMap.h"

MIDILoader::MIDILoader(const char* file) : AbstractMIDILoader(file)
{
	this->file = std::string(file);
	is = nullptr;
	channels = std::array<std::unique_ptr<MIDITrack>, 16>();
	colors = std::array<uint32_t, 16>();

	AddBar([this]() { return (this->pis == nullptr || !this->pis->IsOpened()) ? this->prog : this->pis->GetProgress(); });
}

MIDILoader::MIDILoader(std::shared_ptr<InputStream> is)
	: MIDILoader("")
{
	this->is = is;
}

std::shared_ptr<MIDISequence> MIDILoader::Load()
{
	running = true;

	if (!file.empty())
	{
		auto path = std::filesystem::path(file);
		std::cout << "Loading file " << path.filename() << std::endl;
		MIDIStreamInfo stream = OpenMIDIFileStream(file.c_str());
		seq = std::make_shared<MIDISequence>("TODO");
		pis = stream.stream;
	}
	else
	{
		std::cout << "Loading stream" << std::endl;
		// name = "Unnamed";
		seq = std::make_shared<MIDISequence>();
		pis = std::make_shared<ProgressInputStream>(*(is.get()));
	}

	for (int c = 0; c < 16; ++c)
	{
		if (!channels[c]) channels[c] = std::make_unique<MIDITrack>();
	}

	unendedNotes.reserve(128 << 4);

	SetName("Loading MIDI File");
	unendedNotes.clear();
	currNoteId = 0;
	noteons.clear();

	#pragma region Header Parse
	
	std::vector<uint8_t> header(14);
	pis->Read(header.data(), 14);
	uint8_t* hdrP = header.data();
	if (ToInt(hdrP) != 0x4d546864)
	{
		throw std::exception("Invalid header");
	}
	hdrP += 4;
	if (ToInt(hdrP) != 6)
	{
		throw std::exception("Invalid track length");
	}
	hdrP += 4;
	uint16_t format = ToShort(hdrP);
	hdrP += 2;
	uint16_t tracks = ToShort(hdrP);
	seq->trackCount = tracks;
	hdrP += 2;
	seq->resolution = ToShort(hdrP);
	hdrP += 2;
	seq->tracks.reserve((size_t)tracks * 16);
	
	#pragma endregion

	#pragma region Track Parse

	std::vector<int> illegalTracks{};
	int noteTrackIdx = 0;
	int i;
#ifdef COMET_DEBUG
	try
	{
#endif
	for (i = 0; i < tracks; i++)
	{
		if (!running)
		{
			std::cout << "\n  !! Stop requested. Aborting load and returning unfinished sequence." << std::endl;
			pis->Close();
			return seq;
		}
		std::cout << "\r  Loading new track " << (i + 1) << "/" << tracks << std::endl;
		SetName(("Loading track " + std::to_string(i + 1) + "/" + std::to_string(tracks)).c_str());
		LoadTrack(pis, i);
		for (int o = 0; o < channels.size(); o++)
		{
			if (channels[o])
			{
				channels[o]->color = colors[o];
			}
		}
		int c = 0;
		for (int j = 0; j < channels.size(); j++)
		{
			MIDITrack* channelTrack = channels[j].get();

			if (channelTrack && !channelTrack->notes.empty())
			{
				for (size_t k = 0; k < channelTrack->notes.size(); k++)
				{
					channelTrack->notes[k].track = noteTrackIdx;
				}

				noteTrackIdx++;
				c++;
			}

			if (channels[j])
			{
				seq->tracks.push_back(std::move(*channels[j]));
				channels[j].reset();
			}
		}
		noteons.clear();
		if (c >= 2)
		{
			illegalTracks.push_back(i);
		}
	}
#ifdef COMET_DEBUG
	}
	catch (const std::exception& e)
	{
		std::cout << "Failed to parse MIDI tracks. Aborting parse\nReason: " << e.what() << std::endl;
	}

	pis->Close();
#endif

	#pragma endregion

	#pragma region Event Post-process

	seq->noteTrackCount = noteTrackIdx;
	std::cout << "\nLoaded all tracks, sorting tempo events." << std::endl;
	SetName(("Preprocessing events of " + std::string(GetName())).c_str());
	auto& tempos = seq->tempos;
	std::sort(tempos.begin(), tempos.end(), [](auto& a, auto& b) { return a.tick < b.tick; });
	for (int i = 0; i < seq->tracks.size(); i++)
	{
		prog = (double)i / (double)seq->tracks.size();
		std::cout << "  Preprocessing notes of track " << (i + 1) << "/" << seq->tracks.size() << std::endl;
		auto& track = seq->tracks[i];
		std::sort(track.notes.begin(), track.notes.end(), [](auto& a, auto& b) { return a.tick < b.tick; });
	}
	prog = -1;

	seq->tracks.shrink_to_fit();

	#pragma endregion

	#pragma region Merge events

	SetName("Merging events");
	std::cout << "  Parsing finished! Merging events..." << std::endl;
	std::vector<std::vector<NoteEvent>> toMerge(seq->tracks.size());
	// gather notes from tracks
	i = 0;
	for (auto& tracks : seq->tracks)
	{
		toMerge[i++] = std::move(tracks.notes);
	}
	std::vector<NoteEvent> mergedNotes = SequenceFuncs::FlattenSequence(std::move(toMerge));
	SetName("Finishing up!");
	std::cout << "  Finishing up" << std::endl;
	seq->mergedNotes = SequenceFuncs::DistributeNotes(std::move(mergedNotes));
	// really weird way to do this but ok
	seq->tempoMap = std::make_shared<TempoMap>();
	seq->tempoMap->RebuildTempoMap(seq.get());
	#pragma endregion

#ifdef COMET_DEBUG
	std::cout << "MIDI has successfully loaded." << std::endl;
	std::cout << "  " << seq->tempos.size() << " Tempo events" << std::endl;
	std::cout << "  Notes: " << seq->notes << std::endl;
	std::cout << "  Duration: " << seq->CalcLengthMilliseconds() << "ms" << std::endl;

	if (!illegalTracks.empty())
	{
		std::cout << "  Note events with different channel was mixed in Track ";
		for (int i = 0; i < illegalTracks.size(); i++)
		{
			if (i != 0)
			{
				std::cout << ((i == illegalTracks.size() - 1) ? " and " : ", ");
			}
			std::cout << illegalTracks[i];
		}
		std::cout << ". This means this MIDI can't be loaded in Domino :(" << std::endl;
	}
#endif

	prog = 1;

	return seq;
}

void MIDILoader::LoadTrack(std::shared_ptr<InputStream> is, int track)
{
	unendedNotes.clear();
	currNoteId = 0;
	noteons.clear();

	uint8_t hdr[4];

	is->Read(hdr, 4);
	if (ToInt(hdr) != 0x4D54726B)
		throw std::runtime_error("Invalid track header");

	is->Read(hdr, 4);
	const uint32_t trackLength = ToInt(hdr);

	std::vector<uint8_t> trackData(trackLength);
	is->Read(trackData.data(), trackLength); // may take up more memory than the buffered reader approach but oh well

	const uint8_t* p = trackData.data();
	const uint8_t* end = p + trackLength;

	bool read = true;
	long tick = 0L;
	uint8_t lastStatus = 0;

	try
	{
		while (p < end && read && running)
		{
			tick += (long)ReadVariableLengthValue(p, end);
			if (p >= end) break;
			
			uint8_t b = *p++;

			if (b == 0xFF)
			{
				if (p >= end) break;

				uint8_t type = *p++;
				uint32_t len = ReadVariableLengthValue(p, end);

				if (len > (uint32_t)(end - p))
					throw std::runtime_error("Meta event length exceeds track size");

				switch (type)
				{
					case 0x2F:
						read = false;
						p += len;
						continue;
					case 0x51:
						if (len == 3)
						{
							long msec = ((long)p[0] << 16) | ((long)p[1] << 8) | (long)p[2];
							if (msec == 0)
								throw std::runtime_error("Infinity BPM");
							double bpm = 6.0e7 / (double)msec;
							seq->tempos.emplace_back(tick, bpm);
						}
						p += len;
						continue;
					case 0x0A:
						std::cout << "Color events will be implemented soon" << std::endl;
						p += len;
						continue;
					default:
						p += len;
						continue;
				}
			}

			if (b == 0xF0 || b == 0xF7)
			{
				uint32_t len = ReadVariableLengthValue(p, end);
				if (len > (uint32_t)(end - p))
					throw std::runtime_error("SysEx length exceeds track size");
				p += len;
				continue;
			}

			if (b == 0xF2)
			{
				if ((size_t)(end - p) < 2) break;
				p += 2;
				continue;
			}

			if (b == 0xF3)
			{
				if (p >= end) break;
				++p;
				continue;
			}

			if (b >= 0xF8)
			{
				continue;
			}

			uint8_t status;
			uint8_t data1;

			if ((b & 0x80) != 0)
			{
				status = b;
				lastStatus = status;
				if (p >= end) break;
				data1 = *p++;
			}
			else
			{
				status = lastStatus;
				data1 = b;
			}

			uint8_t channel = (uint8_t)(status & 0x0F);

			switch (status & 0xF0)
			{
				case 0x80:
				{
					if (p >= end) break;
					++p; // velocity, ignored
					if (data1 >= 0x80) continue; // disregard of notes above key 127
					NoteOff(channel, data1, tick);
					continue;
				}

				case 0x90:
				{
					{
						if (p >= end) break;
						uint8_t vel = *p++;

						if (data1 >= 0x80) continue; // disregard of notes above key 127
						if (vel == 0)
						{
							NoteOff(channel, data1, tick);
							continue;
						}

						noteons.emplace_back(track, channel, tick, data1, 0, vel);
						std::stack<size_t>& un = unendedNotes[((uint16_t)(data1) << 4) | (uint16_t)channel];
						un.push(currNoteId);
						currNoteId++;
						continue;
					}
				}
				case 0xA0:
				case 0xB0:
				case 0xE0:
				{
					if (p >= end) break;
					uint8_t val2 = *p++;

					if (loadOnlyNotes) continue;

					GetChannelTrack(channel)->messages.emplace_back(
						tick,
						((uint32_t)val2 << 16) | ((uint32_t)data1 << 8) | (uint32_t)status
					);
					continue;
				}

				case 0xC0:
				case 0xD0:
				{
					if (loadOnlyNotes) continue;

					GetChannelTrack(channel)->messages.emplace_back(
						tick,
						((uint32_t)data1 << 8) | (uint32_t)status
					);
					continue;
				}

				default:
					continue;
			}
		}
	}
	catch (const std::exception& e)
	{
		std::cout << "Error encountered while parsing track. " << e.what() << std::endl;
	}

	seq->length = std::max(seq->length, tick);
}

void MIDILoader::NoteOff(uint8_t ch, uint8_t key, long tick)
{
	std::stack<size_t>& un = unendedNotes[((uint16_t)(key) << 4) | (uint16_t)ch];
	if (un.empty()) return;

	size_t n = un.top();
	un.pop();
	if (n >= noteons.size()) return;

	NoteEvent& note = noteons[n];
	note.gate = tick - note.tick;

	if (tick <= note.tick)
		note.gate = 1;
	else
		note.gate = (uint16_t)((tick - note.tick) & 0xFFFF);

	GetChannelTrack(ch)->notes.push_back(note);
	seq->notes++;
}

MIDITrack* MIDILoader::GetChannelTrack(uint8_t ch)
{
	if (!(channels[ch]))
		channels[ch] = std::make_unique<MIDITrack>();
	return channels[ch].get();
}