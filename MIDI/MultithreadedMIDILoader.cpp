#include "MultithreadedMIDILoader.h"
#include <filesystem>
#include <algorithm>
#include <execution>
#include "Sequence/SequenceFuncs.h"
#include "TempoMap.h"
#include "IO/BufferedByteReader.h"

MultithreadedMIDILoader::MultithreadedMIDILoader(const char* file) : AbstractMIDILoader(file)
{
	this->file = std::string(file);
	is = nullptr;
}

MultithreadedMIDILoader::MultithreadedMIDILoader(std::shared_ptr<InputStream> is)
	: MultithreadedMIDILoader("")
{
	this->is = is;
}

std::shared_ptr<MIDISequence> MultithreadedMIDILoader::Load(bool timeBasedLoading)
{
	running = true;
	tracksProcessed = 0;

	if (!file.empty())
	{
#if __cplusplus >= 202002L
		auto path = std::filesystem::path(reinterpret_cast<const char8_t*>(file.c_str()));
#else
		auto path = std::filesystem::u8path(file);
#endif

		MIDIStreamInfo stream = OpenMIDIFileStream(path);
		seq = std::make_shared<MIDISequence>("TODO");
		pis = stream.stream;
	}
	else
	{
		seq = std::make_shared<MIDISequence>();
		pis = std::make_shared<ProgressInputStream>(*(is.get()));
	}

	SetName("Loading MIDI File...");

#pragma region Header Parse
	std::vector<uint8_t> header(14);
	pis->Read(header.data(), 14);
	uint8_t* hdrP = header.data();
	if (ToInt(hdrP) != 0x4d546864) throw std::runtime_error("Invalid header");
	hdrP += 4;
	if (ToInt(hdrP) != 6) throw std::runtime_error("Invalid header length");
	hdrP += 4;

	uint16_t format = ToShort(hdrP); hdrP += 2;
	uint16_t tracks = ToShort(hdrP); hdrP += 2;
	seq->trackCount = tracks;
	seq->resolution = ToShort(hdrP);
#pragma endregion

#pragma region Sequential Track Ingestion

	std::vector<RawTrackChunk> rawChunks;
	rawChunks.reserve(tracks);

	{
		uint16_t i = 0;

		const auto& progress = [this, &i, tracks]() { return (double)i / (double)tracks; };
		AddBar(progress);

		for (; i < tracks; ++i)
		{
			SetName((std::string("Indexing track ") + std::to_string(i + 1) + "/" + std::to_string(tracks)).c_str());

			if (!running) { pis->Close(); return seq; }
			uint8_t chunkHdr[8];
			pis->Read(chunkHdr, 8);
			if (ToInt(chunkHdr) != 0x4D54726B)
			{
				ClearBars();
				throw std::runtime_error("Invalid track header");
			}

			size_t start = pis->GetPosition();
			size_t trackLength = (size_t)ToInt(chunkHdr + 4);

			RawTrackChunk chunk;
			chunk.start = start;
			chunk.index = i;
			chunk.reader = std::make_unique<BufferedByteReader>(pis->GetStream(), start, trackLength, 1048576, &fileMutex);
			pis->Seek(start + trackLength, SEEK_SET);

			rawChunks.push_back(std::move(chunk));
		}

		ClearBars();
	}
	
#pragma endregion

#pragma region Parallel Track Parsing

	std::vector<ParsedTrack> parsedTrackResults(tracks);

	std::atomic<size_t> currentTrackIdx{ 0 };

	unsigned int hardwareThreads = std::thread::hardware_concurrency();
	if (hardwareThreads == 0) hardwareThreads = 16;
	unsigned int numThreads = std::min((unsigned int)tracks, hardwareThreads);

	std::vector<std::thread> workers;
	workers.reserve(numThreads);

	std::vector<double> progresses(numThreads, 0.0);

	for (unsigned int workerIdx = 0; workerIdx < numThreads; workerIdx++)
	{
		AddBar([&progresses, workerIdx]() {
			return progresses[workerIdx];
			});

		workers.emplace_back([this, &rawChunks, &parsedTrackResults, &currentTrackIdx, tracks, &progresses, workerIdx]() {
			size_t idx;

			while ((idx = currentTrackIdx.fetch_add(1, std::memory_order_relaxed)) < tracks)
			{
				if (!running) break;
				SetName(("Loading track " + std::to_string(idx + 1) + "/" + std::to_string(tracks) + "...").c_str());

				parsedTrackResults[idx] = ParseTrackData(rawChunks[idx]);
				progresses[workerIdx] =
					static_cast<double>(
						tracksProcessed.fetch_add(1, std::memory_order_relaxed) + 1
						) / static_cast<double>(tracks);
			}
			});
	}

	for (auto& worker : workers)
	{
		if (worker.joinable()) worker.join();
	}

	ClearBars();

	pis->Close();
#pragma endregion

#pragma region Sequential Assembly 

	std::vector<int> illegalTracks{};
	
	int noteTrackIdx = 0;
	int trackIdx = 0;

	std::vector<NoteSequence> notesToMerge;
	notesToMerge.reserve((size_t)tracks);

	std::vector<std::vector<MIDIMessageEvent>> eventsToMerge;
	eventsToMerge.reserve((size_t)tracks);

	for (auto& trackRes : parsedTrackResults)
	{
		if (!running) return seq;
		seq->length = std::max(seq->length, trackRes.length);

		if (trackRes.multiChannel)
			illegalTracks.push_back(trackIdx);

		if (!trackRes.notes.Empty())
		{
			seq->notes += trackRes.notes.Size();

			const uint32_t trackMask = NOTE_TRACK_MASK << NOTE_TRACK_BIT_IDX;
			for (uint32_t& packed : trackRes.notes.packed)
				packed = (packed & ~trackMask)
				| ((noteTrackIdx & NOTE_TRACK_MASK) << NOTE_TRACK_BIT_IDX);

			noteTrackIdx++;
			notesToMerge.push_back(std::move(trackRes.notes));
		}

		if (!trackRes.messages.empty())
		{
			eventsToMerge.push_back(std::move(trackRes.messages));
		}

		if (!trackRes.tempos.empty())
		{
			seq->tempos.insert(seq->tempos.end(), trackRes.tempos.begin(), trackRes.tempos.end());
		}

		if (!trackRes.timeSignatures.empty())
		{
			seq->timeSignatures.insert(seq->timeSignatures.end(), trackRes.timeSignatures.begin(), trackRes.timeSignatures.end());
		}
		else
		{
			seq->timeSignatures.emplace_back(0, 4, 4);
		}

		trackIdx++;
	}
#pragma endregion

#pragma region Post-processing & Merging
	seq->noteTrackCount = noteTrackIdx;
	std::cout << "\nLoaded all tracks, sorting tempo events." << std::endl;
	SetName("Starting event preprocessing");
	auto& tempos = seq->tempos;
	auto& timeSignatures = seq->timeSignatures;
	std::sort(std::execution::par_unseq, tempos.begin(), tempos.end(), [](auto& a, auto& b) { return a.tick < b.tick; });
	std::sort(std::execution::par_unseq, timeSignatures.begin(), timeSignatures.end(), [](auto& a, auto& b) { return a.tick < b.tick; });

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

	std::cout << "  Parsing finished! Merging events..." << std::endl;
	SetName("Parsing finished! Merging events...");

	NoteSequence mergedNotes = SequenceFuncs::FlattenSequence(std::move(notesToMerge));
	std::vector<MIDIMessageEvent> mergedEvents = SequenceFuncs::FlattenSequence(std::move(eventsToMerge));

	SetName("Finalizing (1/2)...");
	std::cout << "  Finishing up" << std::endl;
	seq->mergedNotes = SequenceFuncs::DistributeNotes(std::move(mergedNotes));
	seq->mergedEvents = std::move(mergedEvents);

	seq->tempoMap = std::make_shared<TempoMap>();
	seq->tempoMap->RebuildTempoMap(seq.get());

	// NEW: Apply note bounds
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

	SetName("Done!");
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

#pragma endregion
	return seq;
}

AbstractMIDILoader::ParsedTrack MultithreadedMIDILoader::ParseTrackData(const RawTrackChunk& chunk)
{
	auto* reader = chunk.reader.get();

	std::array<std::vector<size_t>, MIDI_KEYS << 4> unendedNotes{};
	size_t currNoteId = 0;

	uint32_t currTick = 0;
	uint8_t prevCommand = 0;
	bool ended = false;

	uint8_t firstChannel = 255;

	ParsedTrack midiTrack;

	while (running && !ended)
	{
		uint32_t delta = ReadVLQ(reader);
		currTick += delta;
		uint8_t command = reader->ReadByte();
		if (command < 0x80)
		{
			reader->Seek(-1, SEEK_CUR);
			command = prevCommand;
		}
		prevCommand = command;

		uint8_t channel = command & 0x0F;
		
		if (!midiTrack.multiChannel && (command & 0xF0) >= 0x80 && (command & 0xF0) <= 0xE0)
		{
			if (firstChannel == 255)
			{
				firstChannel = channel;
			}
			else if (firstChannel != channel)
			{
				midiTrack.multiChannel = true;
			}
		}

		switch (command & 0xF0)
		{
			case 0x80:
			{
				uint8_t key = reader->ReadByte();
				reader->Skip(1);

				if (key >= MIDI_KEYS) continue;

				auto& un = unendedNotes[((size_t)key << 4) | (size_t)channel];
				if (!un.empty())
				{
					size_t n = un.back();
					un.pop_back();
					midiTrack.notes.gate[n] = currTick - midiTrack.notes.tick[n];
				}

				continue;
			}
			case 0x90:
			{
				uint8_t key = reader->ReadByte();
				uint8_t vel = reader->ReadByte();

				if (key >= MIDI_KEYS) continue;

				auto& un = unendedNotes[((size_t)key << 4) | (size_t)channel];

				if (vel > 0)
				{
					midiTrack.notes.Emplace(chunk.index, channel, currTick, key, (uint32_t)(-1), vel);
					un.push_back(currNoteId);
					currNoteId++;
				}
				else if (!un.empty())
				{
					size_t n = un.back();
					un.pop_back();
					midiTrack.notes.gate[n] = currTick - midiTrack.notes.tick[n];
				}
				continue;
			}
			case 0xA0:
			case 0xB0:
			case 0xE0:
			{
				uint8_t data1 = reader->ReadByte();
				uint8_t data2 = reader->ReadByte();

				if (loadOnlyNotes) continue;

				midiTrack.messages.emplace_back(
					currTick,
					((uint32_t)data2 << 16) | ((uint32_t)data1 << 8) | (uint32_t)command
				);
				continue;
			}

			case 0xC0:
			case 0xD0:
			{
				uint8_t data1 = reader->ReadByte();

				if (loadOnlyNotes) continue;

				midiTrack.messages.emplace_back(
					currTick,
					((uint32_t)data1 << 8) | (uint32_t)command
				);
				continue;
			}
			case 0xF0:
			{

				if (command == 0xFF)
				{
					uint8_t type = reader->ReadByte();
					uint32_t len = ReadVLQ(reader);

					switch (type)
					{
					case 0x2F:
						ended = true;
						continue;
					case 0x51:
						if (len == 3)
						{
							long msec = ((long)reader->ReadByte() << 16) | ((long)reader->ReadByte() << 8) | (long)reader->ReadByte();
							if (msec == 0)
								throw std::runtime_error("Infinity BPM");
							double bpm = 6.0e7 / (double)msec;
							midiTrack.tempos.emplace_back(currTick, bpm);
						}
						else
						{
							reader->Skip(len);
						}
						continue;
					case 0x58:
						if (len == 4)
						{
							uint8_t numerator = reader->ReadByte();
							uint8_t denominator = (1 << reader->ReadByte());
							// 3 and 4 are omitted until it's actually needed
							reader->Skip(2);
							midiTrack.timeSignatures.emplace_back(currTick, numerator, denominator);
						}
						else
						{
							reader->Skip(len);
						}

						continue;
					case 0x0A:
					{
						std::cout << "Color events will be implemented soon" << std::endl;
						reader->Skip(len);
						continue;
					}
					default:
						reader->Skip(len);
						continue;
					}
				}

				if (command == 0xF0 || command == 0xF7)
				{
					uint32_t len = ReadVLQ(reader);
					reader->Skip(len);
					continue;
				}

				if (command == 0xF2)
				{
					reader->Skip(2);
					continue;
				}

				if (command == 0xF3)
				{
					reader->Skip(1);
					continue;
				}

				if (command >= 0xF8)
				{
					continue;
				}

				continue;
			}
		}
	}
	midiTrack.length = currTick;
	return midiTrack;
}

uint32_t MultithreadedMIDILoader::ReadVLQ(BufferedByteReader* reader)
{
	uint32_t vlq = 0;
	while (true)
	{
		uint8_t b = reader->ReadByte();
		vlq = (vlq << 7) | ((uint32_t)(b & 0x7F));
		if (!(b & 0x80)) break;
	}
	return vlq;
}