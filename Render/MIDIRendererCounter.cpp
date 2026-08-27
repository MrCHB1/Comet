#include "MIDIRendererCounter.h"
#include "App/MIDIApp.h"
#include "MIDI/TempoMap.h"
#include "MIDI/Timer/MIDITimer.h"

void MIDIRendererCounter::Initialize()
{
	AbstractMIDIRenderer::Initialize();
}

void MIDIRendererCounter::LoadSequence(std::shared_ptr<MIDISequence> sequence)
{
	if (seq != sequence) AbstractMIDIRenderer::UnloadSequence();
	AbstractMIDIRenderer::LoadSequence(sequence);

	seq = sequence;

	for (auto& id : startBlockIDs)
		id = 0;
}

void MIDIRendererCounter::Render(double deltaTime)
{
	if (!seq) return;
	std::vector<NoteSequence>& notes = seq->mergedNotes;
	if (notes.empty()) return;

	double playbackSeconds = app->GetTimer()->Elapsed();
	TempoMap* tempoMap = seq->GetTempoMap();
	long time = tempoMap->SecsToTicksFromMap(seq->resolution, playbackSeconds);
	double bpm = tempoMap->GetBPMAtTick(time);
	noteCounterInfo->tick = time >= 0 ? time : 0;
	noteCounterInfo->timeSeconds = playbackSeconds;
	noteCounterInfo->bpm = bpm;

	const double accTime = isTimeBased ? playbackSeconds : time;
	const double invTimeMultiplier = 1.0 / (double)TIME_BASED_MULTIPLIER;

	size_t notesPassed = 0;
	size_t polyphony = 0;

	for (uint8_t id = 0; id < MIDI_KEYS; id++)
	{
		const NoteSequence& notesNote = notes[id];
		const std::vector<NoteBlock>& blocks = seq->noteBlocks[id];
		if (blocks.empty()) continue;

#pragma region Note block culling

		size_t& blockIndex = startBlockIDs[id];
		size_t noteBegin = blockIndex * NOTE_BLOCK_SIZE;

		if (lastTime < time)
		{
			while (blockIndex < blocks.size())
			{
				double maxBound = blocks[blockIndex].maxBound;
				if (isTimeBased) maxBound *= invTimeMultiplier;

				if (maxBound > accTime) break;
				++blockIndex;
			}
		}
		else if (lastTime > time)
		{
			while (blockIndex > 0)
			{
				double maxBound = blocks[blockIndex - 1].maxBound;
				if (isTimeBased) maxBound *= invTimeMultiplier;

				if (maxBound <= accTime) break;
				--blockIndex;
			}
		}

		noteBegin = std::min(notesNote.Size(), blockIndex * NOTE_BLOCK_SIZE);
		notesPassed += noteBegin;

#pragma endregion

		size_t numNotes = notesNote.Size();
		size_t i = noteBegin;

		while (i < numNotes)
		{
			const size_t blockIndex = i / NOTE_BLOCK_SIZE;
			const size_t blockEnd = std::min(numNotes, (blockIndex + 1) * NOTE_BLOCK_SIZE);
			
			const NoteBlock& currBlock = blocks[blockIndex];
			double blockMin = currBlock.minBound;
			double blockMax = currBlock.maxBound;

			if (isTimeBased)
			{
				blockMin *= invTimeMultiplier;
				blockMax *= invTimeMultiplier;
			}

			if (blockMax <= accTime)
			{
				notesPassed += blockEnd - i;
				i = blockEnd;
				continue;
			}

			if (blockMin > accTime) break;
			bool beyondView = false;

			for (; i < blockEnd; ++i)
			{
				uint32_t nTick = notesNote.tick[i];
				uint32_t nGate = notesNote.gate[i];

				double noteStart = (double)nTick;
				double noteEnd = (double)(nTick + nGate);

				if (isTimeBased)
				{
					noteStart *= invTimeMultiplier;
					noteEnd *= invTimeMultiplier;
				}

				if (noteStart > accTime)
				{
					beyondView = true;
					break;
				}

				if (noteEnd <= accTime)
				{
					notesPassed++;
					continue;
				}

				if (noteStart <= accTime)
				{
					notesPassed++;
					polyphony++;
				}
			}

			if (beyondView) break;
		}
	}

	noteCounterInfo->notesPassed = static_cast<uint64_t>(notesPassed);
	noteCounterInfo->polyphony = static_cast<uint64_t>(polyphony);

	if (!noteCounterInfo->npsHistory.empty() && playbackSeconds < noteCounterInfo->npsHistory.back().timeSeconds)
	{
		noteCounterInfo->npsHistory.clear();
	}

	noteCounterInfo->npsHistory.push_back({ playbackSeconds, static_cast<uint64_t>(notesPassed) });
	while (!noteCounterInfo->npsHistory.empty() &&
		(playbackSeconds - noteCounterInfo->npsHistory.front().timeSeconds) > 1.0)
	{
		noteCounterInfo->npsHistory.pop_front();
	}

	if (!noteCounterInfo->npsHistory.empty())
	{
		uint64_t notesOneSecondAgo = noteCounterInfo->npsHistory.front().totalNotes;
		uint64_t currentNotes = static_cast<uint64_t>(notesPassed);

		if (currentNotes >= notesOneSecondAgo)
		{
			noteCounterInfo->notesPerSecond = currentNotes - notesOneSecondAgo;
		}
		else
		{
			noteCounterInfo->notesPerSecond = 0;
		}
	}
	else
	{
		noteCounterInfo->notesPerSecond = 0;
	}

	lastTime = time;
}

void MIDIRendererCounter::OnResize(int width, int height)
{
	AbstractMIDIRenderer::OnResize(width, height);
}