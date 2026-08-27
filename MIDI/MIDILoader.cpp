// TODO - also have multithreaded parsing as an option

#include "MIDILoader.h"
#include <filesystem>
#include "IO/BufferedByteReader.h"
#include "Comet.h"
#include <algorithm>
#include "Sequence/SequenceFuncs.h"
#include "TempoMap.h"
#include <numeric>

#include "imgui.h"

MIDILoader::MIDILoader(const char* file) : AbstractMIDILoader(file)
{
	this->file = std::string(file);
	is = nullptr;

	AddBar([this]() { return (this->pis == nullptr || !this->pis->IsOpened()) ? this->prog : this->pis->GetProgress(); });
}

MIDILoader::MIDILoader(std::shared_ptr<InputStream> is)
	: MIDILoader("")
{
	this->is = is;
}

std::shared_ptr<MIDISequence> MIDILoader::Load(bool timeBasedLoading)
{
	running = true;

	if (!file.empty())
	{
#if __cplusplus >= 202002L
		auto path = std::filesystem::path(reinterpret_cast<const char8_t*>(file.c_str()));
#else
		auto path = std::filesystem::u8path(file);
#endif
		auto filenameU8 = path.filename().u8string();
		std::cout << "Loading file " << reinterpret_cast<const char*>(filenameU8.c_str()) << std::endl;
		MIDIStreamInfo stream = OpenMIDIFileStream(path);
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

	SetName("Loading MIDI File");

	#pragma region Header Parse
	
	std::vector<uint8_t> header(14);
	pis->Read(header.data(), 14);
	uint8_t* hdrP = header.data();
	if (ToInt(hdrP) != 0x4d546864)
	{
		throw std::runtime_error("Invalid header");
	}
	hdrP += 4;
	if (ToInt(hdrP) != 6)
	{
		throw std::runtime_error("Invalid header length");
	}
	hdrP += 4;
	uint16_t format = ToShort(hdrP);
	hdrP += 2;
	uint16_t tracks = ToShort(hdrP);
	seq->trackCount = tracks;
	hdrP += 2;
	seq->resolution = ToShort(hdrP);
	hdrP += 2;

	#pragma endregion

	#pragma region Track Parse

	std::vector<uint16_t> illegalTracks{};

	std::vector<NoteSequence> notesToMerge;
	notesToMerge.reserve((size_t)tracks);
	std::vector<std::vector<MIDIMessageEvent>> eventsToMerge;
	eventsToMerge.reserve((size_t)tracks);

#ifdef COMET_DEBUG
	try
	{
#endif
	size_t noteTrackIdx = 0;
	for (uint16_t trk = 0; trk < tracks; trk++)
	{
		if (!running)
		{
			std::cout << "\n  !! Stop requested. Aborting load and returning unfinished sequence." << std::endl;
			pis->Close();
			return seq;
		}
		std::cout << "\r  Loading new track " << (trk + 1) << "/" << tracks << std::endl;
		SetName(("Loading track " + std::to_string(trk + 1) + "/" + std::to_string(tracks)).c_str());

		ParsedTrack results = LoadTrack(pis, noteTrackIdx);
		
		if (!results.notes.Empty())
		{
			seq->notes += results.notes.Size();
			notesToMerge.push_back(std::move(results.notes));
			noteTrackIdx++;
		}

		if (!results.messages.empty())
		{
			eventsToMerge.push_back(std::move(results.messages));
		}

		if (!results.tempos.empty())
		{
			seq->tempos.insert(seq->tempos.end(), results.tempos.begin(), results.tempos.end());
		}

		if (!results.timeSignatures.empty())
		{
			seq->timeSignatures.insert(seq->timeSignatures.end(), results.timeSignatures.begin(), results.timeSignatures.end());
		}
		else
		{
			seq->timeSignatures.emplace_back(0, 4, 4);
		}
		if (results.multiChannel) illegalTracks.push_back(trk);
		seq->length = std::max(seq->length, results.length);
	}
#ifdef COMET_DEBUG
	}
	catch (const std::runtime_error& e)
	{
		std::cout << "Failed to parse MIDI tracks. Aborting parse\nReason: " << e.what() << std::endl;
	}

	pis->Close();
#endif

	#pragma endregion

	#pragma region Event Post-process

	seq->noteTrackCount = noteTrackIdx;
	std::cout << "\nLoaded all tracks, sorting tempo events." << std::endl;
	SetName("Starting event preprocessing");
	auto& tempos = seq->tempos;
	auto& timeSignatures = seq->timeSignatures;
	std::sort(tempos.begin(), tempos.end(), [](const auto& a, const auto& b) { return a.tick < b.tick; });
	std::sort(timeSignatures.begin(), timeSignatures.end(), [](const auto& a, const auto& b) { return a.tick < b.tick; });

	prog = -1;

	// handle edge cases for tempos
	if (tempos.empty())
	{
		tempos.emplace_back(0, 120.0);
	}
	else if (tempos.front().tick > 0)
	{
		tempos.insert(tempos.begin(), TempoEvent(0, 120.0));
	}

	if (timeSignatures.empty())
	{
		timeSignatures.emplace_back(0, 4, 4);
	}
	else if (timeSignatures.front().tick > 0)
	{
		timeSignatures.insert(timeSignatures.begin(), TimeSignatureEvent(0, 4, 4));
	}

	seq->tracks.shrink_to_fit();
	#pragma endregion

	#pragma region Merge events
	SetName("Merging events...");
	std::cout << "  Parsing finished! Merging events..." << std::endl;

	NoteSequence mergedNotes = SequenceFuncs::FlattenSequence(std::move(notesToMerge));
	std::vector<MIDIMessageEvent> mergedEvents = SequenceFuncs::FlattenSequence(std::move(eventsToMerge));

	SetName("Finalizing (1/2)...");
	std::cout << "  Finishing up" << std::endl;
	seq->mergedNotes = SequenceFuncs::DistributeNotes(std::move(mergedNotes));
	seq->mergedEvents = std::move(mergedEvents);
	// really weird way to do this but ok
	seq->tempoMap = std::make_shared<TempoMap>();
	seq->tempoMap->RebuildTempoMap(seq.get());

	// NEW: apply note bounds
	ClearBars();

#pragma region Time-based loading if enabled
	if (timeBasedLoading)
	{
		std::cout << "Applying tempo events..." << std::endl;
		std::cout << "  Processing notes" << std::endl;
		TempoMap* tempoMap = seq->GetTempoMap();
		for (auto& notes : seq->mergedNotes)
		{
			SequenceFuncs::ApplyTempoEvents(seq->resolution, tempoMap, notes);
		}
		std::cout << "  Processing events" << std::endl;
		SequenceFuncs::ApplyTempoEvents(seq->resolution, tempoMap, seq->mergedEvents);
		SequenceFuncs::ApplyTempoEvents(seq->resolution, tempoMap, seq->timeSignatures);
	}
	seq->timeBased = timeBasedLoading;
#pragma endregion

	SetName("Finalizing (2/2)...\nIndexing note blocks...");
	size_t blockProgress = 0;
	AddBar([this, &blockProgress]() { return (double)blockProgress / (double)seq->notes; });

	std::array<std::vector<NoteBlock>, MIDI_KEYS> noteBlockArray{};

	size_t key = 0;
	for (const NoteSequence& notes : seq->mergedNotes)
	{
		std::vector<NoteBlock> blocks{};
		blocks.reserve((notes.Size() + NOTE_BLOCK_SIZE - 1) / NOTE_BLOCK_SIZE);

		uint32_t blockMin = (uint32_t)(-1); // funny underflow hack
		uint32_t blockMax = 0;

		for (size_t i = 0; i < notes.Size(); ++i)
		{
			uint32_t tick = notes.tick[i];
			uint32_t tickEnd = tick + notes.gate[i];

			blockMin = std::min(blockMin, tick);
			blockMax = std::max(blockMax, tickEnd);

			++blockProgress;

			if ((i + 1) % NOTE_BLOCK_SIZE == 0)
			{
				blocks.emplace_back(blockMin, blockMax);
				blockMin = (uint32_t)(-1);
				blockMax = 0;
			}
		}

		if (notes.Size() % NOTE_BLOCK_SIZE != 0)
		{
			blocks.emplace_back(blockMin, blockMax);
		}

		noteBlockArray[key++] = std::move(blocks);
	}
	seq->noteBlocks = std::move(noteBlockArray);

	ClearBars();

	#pragma endregion

	std::cout << "MIDI has successfully loaded." << std::endl;
	std::cout << "  " << seq->tempos.size() << " Tempo events" << std::endl;
	std::cout << "  " << seq->timeSignatures.size() << " Time signature events" << std::endl;
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

	prog = 1;

	return seq;
}

AbstractMIDILoader::ParsedTrack MIDILoader::LoadTrack(std::shared_ptr<InputStream> is, int track)
{
	std::array<std::vector<size_t>, 2048> unendedNotes{};
	size_t currNoteId = 0;

	uint8_t hdr[4];

	is->Read(hdr, 4);
	if (ToInt(hdr) != 0x4D54726B)
		throw std::runtime_error("Invalid track header");

	is->Read(hdr, 4);
	const uint32_t trackLength = ToInt(hdr);

	auto trackData = std::make_unique<uint8_t[]>(trackLength);
	is->Read(trackData.get(), trackLength); // may take up more memory than the buffered reader approach but oh well

	const uint8_t* p = trackData.get();
	const uint8_t* end = p + trackLength;

	bool read = true;
	long tick = 0L;

	uint8_t lastStatus = 0;
	uint8_t firstChannel = 255;

	ParsedTrack midiTrack;

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
							midiTrack.tempos.emplace_back(tick, bpm);
						}
						p += len;
						continue;
					case 0x58:
						if (len == 4)
						{
							uint8_t numerator = p[0];
							uint8_t denominator = (1 << p[1]);
							// 3 and 4 are omitted until it's actually needed
							midiTrack.timeSignatures.emplace_back(tick, numerator, denominator);
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
			if (!midiTrack.multiChannel && ((status & 0xF0) >= 0x80 && (status & 0xF0) <= 0xE0))
			{
				if (firstChannel == 255)
				{
					firstChannel = channel;
				}
				else if (channel != firstChannel)
				{
					midiTrack.multiChannel = true;
				}
			}

			switch (status & 0xF0)
			{
				case 0x80:
				{
					if (p >= end) break;
					++p; // velocity, ignored
					if (data1 >= MIDI_KEYS) continue; // disregard of notes above key 127

					auto& un = unendedNotes[((size_t)data1 << 4) | (size_t)channel];
					if (!un.empty())
					{
						size_t n = un.back();
						un.pop_back();
						midiTrack.notes.gate[n] = tick - midiTrack.notes.tick[n];
					}

					continue;
				}

				case 0x90:
				{
					{
						if (p >= end) break;
						uint8_t vel = *p++;

						if (data1 >= MIDI_KEYS) continue; // disregard of notes above key 127
						
						auto& un = unendedNotes[((size_t)data1 << 4) | (size_t)channel];

						if (vel > 0)
						{
							midiTrack.notes.Emplace(track, channel, tick, data1, (uint32_t)(-1), vel);
							un.push_back(currNoteId);
							currNoteId++;
						}
						else if (!un.empty())
						{
							size_t n = un.back();
							un.pop_back();
							midiTrack.notes.gate[n] = tick - midiTrack.notes.tick[n];
						}

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

					midiTrack.messages.emplace_back(
						tick,
						((uint32_t)val2 << 16) | ((uint32_t)data1 << 8) | (uint32_t)status
					);
					continue;
				}

				case 0xC0:
				case 0xD0:
				{
					if (loadOnlyNotes) continue;

					midiTrack.messages.emplace_back(
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
	catch (const std::runtime_error& e)
	{
		std::cout << "Error encountered while parsing track. " << e.what() << std::endl;
	}

	midiTrack.length = tick;
	return midiTrack;
}
