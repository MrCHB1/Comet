#include "MultithreadedMIDILoader.h"
#include <filesystem>
#include <algorithm>
#include <execution>
#include "Sequence/SequenceFuncs.h"
#include "TempoMap.h"
#include "IO/BufferedByteReader.h"
#include "Utils.h"
#include "Concurrency/Utils.h"

namespace
{
	template <typename F>
	void WithBars(Progress* progress, std::vector<std::function<double()>> funcs, F&& func)
	{
		for (auto& f : funcs) progress->AddBar(f);
		func();
		progress->ClearBars();
	}
}

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

std::unique_ptr<MIDISequence> MultithreadedMIDILoader::Load(bool timeBasedLoading)
{
	running = true;
	std::unique_ptr<MIDISequence> seq;

	tracksProcessed = 0;

	if (!file.empty())
	{
#if __cplusplus >= 202002L
		auto path = std::filesystem::path(reinterpret_cast<const char8_t*>(file.c_str()));
#else
		auto path = std::filesystem::u8path(file);
#endif

		MIDIStreamInfo stream = OpenMIDIFileStream(path);
		seq = std::make_unique<MIDISequence>("TODO");
		pis = stream.stream;
	}
	else
	{
		seq = std::make_unique<MIDISequence>();
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

		uint8_t chunkHdr[8];
		for (; i < tracks; ++i)
		{
			SetName((std::string("Indexing track ") + std::to_string(i + 1) + "/" + std::to_string(tracks)).c_str());

			if (!running) { pis->Close(); return nullptr; }
			
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
			chunk.length = trackLength;
			pis->Seek(start + trackLength, SEEK_SET);

			rawChunks.push_back(std::move(chunk));
		}

		ClearBars();
	}
	
#pragma endregion

#pragma region Parallel Track Parsing

	std::vector<ParsedTrack> parsedTrackResults(tracks);

	std::atomic<size_t> processedTracks{ 0 };
	WithBars(this, {
		[this, &processedTracks, &tracks]()
		{ 
			SetName(std::format("Parsing track {}/{}...", processedTracks.load(), tracks).c_str());
			return (double)processedTracks.load() / (double)tracks;
		} },
		[&]()
		{
			Utils::ParForEach(rawChunks.begin(), rawChunks.end(),
				[&](RawTrackChunk& chunk)
				{
					if (!running) return;
					parsedTrackResults[chunk.index] = ParseTrackData(chunk);
					processedTracks.fetch_add(1, std::memory_order_relaxed);
				});
		});

	pis->Close();
	if (!running) return nullptr;
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

	size_t processed = 0;
	size_t total = 0;
	
	NoteSequence mergedNotes{};
	std::vector<MIDIMessageEvent> mergedEvents;

	{
		std::string phase = "";
		WithBars(this, { [&]() { return (double)processed / (double)total; } },
			[&]()
			{
				SetName("Merging notes...");
				total = seq->notes;
				mergedNotes = SequenceFuncs::FlattenSequence(std::move(notesToMerge), &processed);

				total = 0;
				SetName("Merging events...");
				for (const std::vector<MIDIMessageEvent>& evs : eventsToMerge)
					total += evs.size();
				mergedEvents = SequenceFuncs::FlattenSequence(std::move(eventsToMerge), &processed);
			});
	}
	
	if (!running) return nullptr;

	std::cout << "  Finishing up" << std::endl;

	size_t notesProcessed = 0;
	WithBars(this,
		{ [&notesProcessed, &seq]() { return seq->notes ? (double)notesProcessed / (double)seq->notes : 1.0; } },
		[&]() { seq->mergedNotes = SequenceFuncs::DistributeNotes(std::move(mergedNotes), &notesProcessed); }
	);

	SetName("Merging events...");
	seq->mergedEvents = std::move(mergedEvents);

	SetName("Building tempo map...");
	seq->tempoMap = std::make_unique<TempoMap>();
	seq->tempoMap->RebuildTempoMap(seq.get());

	if (!running) return nullptr;

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

	SetName("Indexing note blocks...");

	size_t blockProgress = 0;
	WithBars(this,
		{ [this, &blockProgress, &seq]() { return (double)blockProgress / (double)seq->notes; } },
		[&]()
		{
			std::array<std::vector<NoteBlock>, MIDI_KEYS> noteBlockArray{};

			size_t key = 0;
			for (const NoteSequence& notes : seq->mergedNotes)
			{
				if (!running) return;

				std::vector<NoteBlock> blocks{};
				blocks.reserve((notes.Size() + NOTE_BLOCK_SIZE - 1) / NOTE_BLOCK_SIZE);

				uint32_t blockMin = (uint32_t)(-1); // funny underflow hack
				uint32_t blockMax = 0;

				for (size_t i = 0; i < notes.Size(); ++i)
				{
					if (!running) return;

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
		});

	if (!running) return nullptr;

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

AbstractMIDILoader::ParsedTrack MultithreadedMIDILoader::ParseTrackData(RawTrackChunk& chunk)
{
	static constexpr size_t kNoteInitialReserve = 1 << 12;
	static constexpr size_t kEvtInitialReserve = 1 << 8;
	std::vector<std::vector<uint32_t>> unendedNotes(MIDI_KEYS << 4);
	std::ifstream* stream = pis->GetStream().get();

	auto trackBytes = std::make_unique_for_overwrite<uint8_t[]>(chunk.length);
	{
		std::lock_guard<std::mutex> lock(fileMutex);

		stream->seekg(chunk.start, std::ios::beg);
		stream->read(reinterpret_cast<char*>(trackBytes.get()), static_cast<std::streamsize>(chunk.length));
	}
	const uint8_t* trackPtr = trackBytes.get();
	const uint8_t* end = trackPtr + chunk.length;

	uint32_t currTick = 0;
	uint8_t prevCommand = 0;
	bool ended = false;

	uint8_t firstChannel = 255;

	ParsedTrack midiTrack;

	// -------------------------------------
	// PASS 1: count notes + events + analysis
	// -------------------------------------
	size_t nc = 0, ec = 0, tc = 0, tsc = 0;
	while (running && !ended && trackPtr < end)
	{
		while (*trackPtr++ & 0x80) {} // fast vlq read
		int pending = -1;
		uint8_t command = *(trackPtr++);

		if (command < 0x80) { pending = command; command = prevCommand; }
		else if (command < 0xF0) prevCommand = command;

		auto d1 = [&]() -> uint8_t {
			if (pending >= 0) { auto v = (uint8_t)pending; pending = -1; return v; }
			return *(trackPtr++);
		};

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
			trackPtr += 2;
			continue;
		case 0x90:
		{
			uint8_t key = d1();
			uint8_t vel = *(trackPtr++);
			if (key >= MIDI_KEYS || vel == 0) continue;
			nc++;
			continue;
		}
		case 0xA0:
		case 0xB0:
		case 0xE0:
		{
			d1();
			trackPtr++;
			if (!loadOnlyNotes) ec++;
			continue;
		}
		case 0xC0:
		case 0xD0:
		{
			d1();
			if (!loadOnlyNotes) ec++;
			continue;
		}
		case 0xF0:
		{
			if (command == 0xFF)
			{
				uint8_t type = *trackPtr++;
				uint32_t len = ReadVLQ(trackPtr);

				switch (type)
				{
				case 0x2F:
					ended = true;
					continue;
				case 0x51:
					if (len == 3) tc++;
					trackPtr += len;
					continue;
				case 0x58:
					if (len == 4) tsc++;
					trackPtr += len;
					continue;
				default:
					trackPtr += len;
					continue;
				}
			}

			if (command == 0xF0 || command == 0xF7)
			{
				uint32_t len = ReadVLQ(trackPtr);
				trackPtr += len;
				continue;
			}
			if (command == 0xF2) { trackPtr += 2; continue; }
			if (command == 0xF3) { trackPtr++; continue; }
			if (command >= 0xF8) continue;

			continue;
		}
		}
	}
	
	if (!running) return {};

	trackPtr = trackBytes.get();
	ended = false;
	prevCommand = 0;
	currTick = 0;

	// -------------------------------------
	// PASS 2: event population
	// -------------------------------------

	// reserve what we counted
	try
	{
		midiTrack.notes.Reserve(nc);
	}
	catch (const std::bad_alloc& e)
	{
		throw std::runtime_error("Not enough memory to allocate notes.");
	}
	
	auto tryAlloc = [&](auto& vec, size_t count) -> bool
		{
			try { vec.reserve(count); return true; }
			catch (const std::bad_alloc& e) { return false; }
		};

	if (!tryAlloc(midiTrack.messages, ec))
		throw std::runtime_error("Not enough memory to allocate MIDI messages.");

	if (!tryAlloc(midiTrack.tempos, tc))
		throw std::runtime_error("Not enough memory to allocate Tempo events.");

	if (!tryAlloc(midiTrack.timeSignatures, tsc))
		throw std::runtime_error("not enough memory to allocate Time signature events.");

	while (running && !ended && trackPtr < end)
	{
		uint32_t delta = ReadVLQ(trackPtr);
		currTick += delta;
		int pending = -1;
		uint8_t command = *(trackPtr++);

		if (command < 0x80) { pending = command; command = prevCommand; }
		else if (command < 0xF0) prevCommand = command;

		auto d1 = [&]() -> uint8_t {
			if (pending >= 0) { auto v = (uint8_t)pending; pending = -1; return v; }
			return *(trackPtr++);
		};

		uint8_t channel = command & 0x0F;
		switch (command & 0xF0)
		{
		case 0x80:
		{
			uint8_t key = d1();
			trackPtr++;

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
			uint8_t key = d1();
			uint8_t vel = *(trackPtr++);

			if (key >= MIDI_KEYS) continue;

			auto& un = unendedNotes[((size_t)key << 4) | (size_t)channel];

			if (vel > 0)
			{
				auto& notes = midiTrack.notes;
				if (notes.tick.capacity() == 0) notes.Reserve(kNoteInitialReserve);
				un.push_back(notes.Size());
				notes.Emplace(chunk.index, channel, currTick, key, (uint32_t)(-1), vel);
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
			uint8_t data1 = d1();
			uint8_t data2 = *trackPtr++;

			if (loadOnlyNotes) continue;

			if (midiTrack.messages.capacity() == 0) midiTrack.messages.reserve(kEvtInitialReserve);
			midiTrack.messages.emplace_back(
				currTick,
				((uint32_t)data2 << 16) | ((uint32_t)data1 << 8) | (uint32_t)command
			);
			continue;
		}

		case 0xC0:
		case 0xD0:
		{
			uint8_t data1 = d1();
			if (loadOnlyNotes) continue;

			if (midiTrack.messages.capacity() == 0) midiTrack.messages.reserve(kEvtInitialReserve);
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
				uint8_t type = *trackPtr++;
				uint32_t len = ReadVLQ(trackPtr);

				switch (type)
				{
				case 0x2F:
					ended = true;
					continue;
				case 0x51:
					if (len == 3)
					{
						long msec = ((long)trackPtr[0] << 16) |
							        ((long)trackPtr[1] << 8) |
								    ((long)trackPtr[2]);
						if (msec == 0)
							throw std::runtime_error("Infinity BPM");
						double bpm = 6.0e7 / (double)msec;
						midiTrack.tempos.emplace_back(currTick, bpm);
					}
					trackPtr += len;
					continue;
				case 0x58:
					if (len == 4)
					{
						uint8_t numerator = *(trackPtr++);
						uint8_t denominator = (1 << (*(trackPtr++)));
						// 3 and 4 are omitted until it's actually needed
						trackPtr += 2;
						midiTrack.timeSignatures.emplace_back(currTick, numerator, denominator);
					}
					else
					{
						trackPtr += len;
					}

					continue;
				case 0x0A:
				{
					std::cout << "Color events will be implemented soon" << std::endl;
					trackPtr += len;
					continue;
				}
				default:
					trackPtr += len;
					continue;
				}
			}

			if (command == 0xF0 || command == 0xF7)
			{
				uint32_t len = ReadVLQ(trackPtr);
				trackPtr += len;
				continue;
			}

			if (command == 0xF2)
			{
				trackPtr += 2;
				continue;
			}

			if (command == 0xF3)
			{
				trackPtr++;
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

	// close off unended notes
	for (size_t slot = 0; slot < unendedNotes.size(); ++slot)
	{
		for (uint32_t n : unendedNotes[slot])
			midiTrack.notes.gate[n] = currTick - midiTrack.notes.tick[n];
	}

	midiTrack.length = currTick;

	if (midiTrack.messages.capacity() > midiTrack.messages.size()) midiTrack.messages.shrink_to_fit();

	// shrink when actually necessary
	auto& notes = midiTrack.notes;
	if (notes.tick.capacity() > notes.tick.size())
	{
		notes.tick.shrink_to_fit();
		notes.gate.shrink_to_fit();
		notes.packed.shrink_to_fit();
	}

	return midiTrack;
}

uint32_t MultithreadedMIDILoader::ReadVLQ(const uint8_t*& bytesPtr)
{
	uint32_t vlq = 0;
	while (true)
	{
		uint8_t b = *bytesPtr++;
		vlq = (vlq << 7) | ((uint32_t)(b & 0x7F));
		if (!(b & 0x80)) break;
	}
	return vlq;
}