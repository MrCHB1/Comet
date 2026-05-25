#include "MIDISequence.h"

double MIDISequence::CalcLengthMilliseconds()
{
	double msTotal = 0;
	long tick = 0L;
	double bpm = 120;
	for (int i = 0; i < tempos.size(); i++)
	{
		TempoEvent* t = &(tempos[i]);
		msTotal += 60000 / bpm * (t->tick - tick) / resolution;
		tick = t->tick;
		bpm = t->bpm;
	}
	return msTotal + 60000.0 / bpm * (length - tick) / resolution;
}