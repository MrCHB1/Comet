#include "MultithreadedMIDILoader.h"
#include <filesystem>
#include <algorithm>
#include <execution>
#include "Sequence/SequenceFuncs.h"
#include "TempoMap.h"

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
		MIDIStreamInfo stream = OpenMIDIFileStream(file.c_str());
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

	for (uint16_t i = 0; i < tracks; ++i)
	{
		if (!running) { pis->Close(); return seq; }
		uint8_t chunkHdr[8];
		pis->Read(chunkHdr, 8);
		if (ToInt(chunkHdr) != 0x4D54726B) throw std::runtime_error("Invalid track header");

		uint32_t trackLength = ToInt(chunkHdr + 4);

		RawTrackChunk chunk;
		chunk.index = i;
		chunk.data.resize(trackLength);
		pis->Read(chunk.data.data(), trackLength);

		rawChunks.push_back(std::move(chunk));
	}
	pis->Close();
	#pragma endregion

	#pragma region Parallel Track Parsing

	std::vector<ParsedTrackResult> parsedTrackResults(tracks);
	std::atomic<size_t> currentTrackIdx{ 0 };

	unsigned int hardwareThreads = std::thread::hardware_concurrency();
	if (hardwareThreads == 0) hardwareThreads = 4;
	unsigned int numThreads = std::min((unsigned int)tracks, hardwareThreads);

	std::vector<std::thread> workers;
	workers.reserve(numThreads);

	for (unsigned int i = 0; i < numThreads; i++)
	{
		workers.emplace_back([this, &rawChunks, &parsedTrackResults, &currentTrackIdx, tracks]() {
			size_t idx;

			while ((idx = currentTrackIdx.fetch_add(1, std::memory_order_relaxed)) < tracks)
			{
				if (!running) break;
				SetName(("Loading track " + std::to_string(idx + 1) + "/" + std::to_string(tracks) + "...").c_str());

				parsedTrackResults[idx] = ParseTrackData(rawChunks[idx]);
				tracksProcessed++;
			}
		});
	}

	for (auto& worker : workers)
	{
		if (worker.joinable()) worker.join();
	}

	#pragma endregion
	
	#pragma region Sequential Assembly 
	// Step 3: Combine parsed chunks back into the main structural sequence sequentially.
	std::vector<int> illegalTracks{};
	int noteTrackIdx = 0;

	seq->tracks.reserve((size_t)tracks * 16);

	for (auto& trackRes : parsedTrackResults)
	{
		if (!running) return seq;
		seq->length = std::max(seq->length, trackRes.length);
		seq->notes += trackRes.numNotes;

		int activeChannelsInTrack = 0;
		for (auto& channelTrack : trackRes.channelTracks)
		{
			if (!channelTrack.notes.empty())
			{
				for (auto& note : channelTrack.notes)
				{
					note.track = noteTrackIdx;
				}
				noteTrackIdx++;
				activeChannelsInTrack++;
			}

			if (!channelTrack.notes.empty() || !channelTrack.messages.empty())
			{
				seq->tracks.push_back(std::move(channelTrack));
			}
		}

		// Collect parsed global tempo events from tracks
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

		if (activeChannelsInTrack >= 2)
		{
			illegalTracks.push_back(trackRes.trackIndex);
		}
	}
	#pragma endregion
	
	#pragma region Post-processing & Merging
	seq->noteTrackCount = noteTrackIdx;

	std::sort(std::execution::par_unseq, seq->tempos.begin(), seq->tempos.end(), [](auto& a, auto& b) { return a.tick < b.tick; });
	std::sort(std::execution::par_unseq, seq->timeSignatures.begin(), seq->timeSignatures.end(), [](auto& a, auto& b) { return a.tick < b.tick; });

	for (size_t i = 0; i < seq->tracks.size(); i++)
	{
		std::cout << "  Preprocessing notes of track " << i << "/" << seq->tracks.size() << std::endl;
		std::sort(std::execution::par_unseq, seq->tracks[i].notes.begin(), seq->tracks[i].notes.end(), [](auto& a, auto& b) { return a.tick < b.tick; });
	}

	seq->tracks.shrink_to_fit();

	std::cout << "  Parsing finished! Merging events..." << std::endl;

	std::vector<std::vector<NoteEvent>> toMerge(seq->tracks.size());
	std::vector<std::vector<MIDIMessageEvent>> eventsToMerge(seq->tracks.size());

	for (size_t i = 0; i < seq->tracks.size(); ++i)
	{
		eventsToMerge[i] = std::move(seq->tracks[i].messages);
		toMerge[i] = std::move(seq->tracks[i].notes);
	}

	std::vector<NoteEvent> mergedNotes = SequenceFuncs::FlattenSequence(std::move(toMerge));
	std::vector<MIDIMessageEvent> mergedEvents = SequenceFuncs::FlattenSequence(std::move(eventsToMerge));

	seq->mergedNotes = SequenceFuncs::DistributeNotes(std::move(mergedNotes));
	seq->mergedEvents = std::move(mergedEvents);

	seq->tempoMap = std::make_shared<TempoMap>();
	seq->tempoMap->RebuildTempoMap(seq.get());

	if (timeBasedLoading)
	{
		TempoMap* tempoMap = seq->GetTempoMap();
		for (auto& notes : seq->mergedNotes)
		{
			SequenceFuncs::ApplyTempoEvents(seq->resolution, tempoMap, notes);
		}
		SequenceFuncs::ApplyTempoEvents(seq->resolution, tempoMap, seq->mergedEvents);
		SequenceFuncs::ApplyTempoEvents(seq->resolution, tempoMap, seq->timeSignatures);
	}

	seq->timeBased = timeBasedLoading;

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

MultithreadedMIDILoader::ParsedTrackResult MultithreadedMIDILoader::ParseTrackData(const RawTrackChunk& chunk)
{
	ParsedTrackResult result;
	result.trackIndex = chunk.index;
	result.channelTracks.resize(16);

	// Pre-allocation heuristic for Black MIDIs to drastically lower re-alloc overhead
	size_t estimatedNotesPerChannel = chunk.data.size() / 16 / 4;
	for (int c = 0; c < 16; ++c)
	{
		result.channelTracks[c].notes.reserve(estimatedNotesPerChannel);
		if (!loadOnlyNotes) result.channelTracks[c].messages.reserve(estimatedNotesPerChannel / 2);
	}

	// Completely isolated state storage mapping per thread
	std::array<std::vector<size_t>, 2048> threadUnendedNotes{};
	std::vector<NoteEvent> threadNoteOns;
	threadNoteOns.reserve(chunk.data.size() / 4);

	size_t localCurrNoteId = 0;
	const uint8_t* p = chunk.data.data();
	const uint8_t* end = p + chunk.data.size();

	bool read = true;
	long tick = 0L;
	uint8_t lastStatus = 0;

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
					if (msec != 0)
					{
						double bpm = 6.0e7 / (double)msec;
						result.tempos.emplace_back(tick, bpm);
					}
				}
				p += len;
				continue;
			case 0x58:
				if (len == 4)
				{
					uint8_t numerator = p[0];
					uint8_t denominator = (1 << p[1]);
					result.timeSignatures.emplace_back(tick, numerator, denominator);
				}
				p += len;
				continue;
			default:
				p += len;
				continue;
			}
		}

		if (b == 0xF0 || b == 0xF7)
		{
			p += ReadVariableLengthValue(p, end);
			continue;
		}
		if (b == 0xF2) { p += 2; continue; }
		if (b == 0xF3) { ++p; continue; }
		if (b >= 0xF8) continue;

		uint8_t status, data1;
		if ((b & 0x80) != 0)
		{
			status = b;
			lastStatus = status;
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
			++p; // Skip velocity
			if (data1 >= 0x80) continue;

			uint16_t idx = ((uint16_t)data1 << 4) | channel;
			if (!threadUnendedNotes[idx].empty())
			{
				size_t n = threadUnendedNotes[idx].back();
				threadUnendedNotes[idx].pop_back();
				if (n < threadNoteOns.size())
				{
					NoteEvent& note = threadNoteOns[n];
					note.gate = (tick <= note.tick) ? 1 : (uint16_t)((tick - note.tick) & 0xFFFF);
					result.channelTracks[channel].notes.push_back(note);
					result.numNotes++;
				}
			}
			continue;
		}
		case 0x90:
		{
			uint8_t vel = *p++;
			if (data1 >= 0x80) continue;
			if (vel == 0)
			{
				uint16_t idx = ((uint16_t)data1 << 4) | channel;
				if (!threadUnendedNotes[idx].empty())
				{
					size_t n = threadUnendedNotes[idx].back();
					threadUnendedNotes[idx].pop_back();
					if (n < threadNoteOns.size())
					{
						NoteEvent& note = threadNoteOns[n];
						note.gate = (tick <= note.tick) ? 1 : (uint16_t)((tick - note.tick) & 0xFFFF);
						result.channelTracks[channel].notes.push_back(note);
						result.numNotes++;
					}
				}
				continue;
			}

			threadNoteOns.emplace_back(chunk.index, channel, tick, data1, 0, vel);
			uint16_t index = ((uint16_t)data1 << 4) | channel;
			threadUnendedNotes[index].push_back(localCurrNoteId++);
			continue;
		}
		case 0xA0:
		case 0xB0:
		case 0xE0:
		{
			uint8_t val2 = *p++;
			if (loadOnlyNotes) continue;
			result.channelTracks[channel].messages.emplace_back(tick, ((uint32_t)val2 << 16) | ((uint32_t)data1 << 8) | (uint32_t)status);
			continue;
		}
		case 0xC0:
		case 0xD0:
		{
			if (loadOnlyNotes) continue;
			result.channelTracks[channel].messages.emplace_back(tick, ((uint32_t)data1 << 8) | (uint32_t)status);
			continue;
		}
		default:
			continue;
		}
	}

	result.length = tick;
	return result;
}