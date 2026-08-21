#include "NoteCuller.h"
#include "MIDI/MIDISequence.h"

void NoteCuller::CalculateCull(MIDISequence* seq, RenderView* renderView, long midiTime, double timeNorm)
{
	if (!seq) return;

	if (cullDirty)
	{
		ResetCull();
		cullDirty = false;
	}

	const double invTimeMultiplier = 1.0 / (double)TIME_BASED_MULTIPLIER;

	notesPassed = 0;

	for (size_t key = 0; key < MIDI_KEYS; key++)
	{
		size_t noteBegin = startRenderIDs[key];

		const NoteSequence& notesNote = seq->mergedNotes[key];
		if (lastTime < midiTime)
		{
			while (noteBegin < notesNote.Size())
			{
				double noteEnd = seq->timeBased
					? (double)(notesNote.tick[noteBegin] + notesNote.gate[noteBegin]) * invTimeMultiplier
					: (double)(notesNote.tick[noteBegin] + notesNote.gate[noteBegin]);

				if (noteEnd > timeNorm) break;
				++noteBegin;
			}
		}
		else if (lastTime > midiTime)
		{
			while (noteBegin > 0)
			{
				size_t prev = noteBegin - 1;
				double noteEnd = seq->timeBased
					? (double)(notesNote.tick[prev] + notesNote.gate[prev]) * invTimeMultiplier
					: (double)(notesNote.tick[prev] + notesNote.gate[prev]);

				if (noteEnd <= timeNorm) break;
				--noteBegin;
			}
		}

		startRenderIDs[key] = noteBegin;
		notesPassed += noteBegin;
	}

	lastTime = midiTime;
}