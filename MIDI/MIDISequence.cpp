#include "MIDISequence.h"

int MIDISequence::GetAudioPosition(long tick, int frequency)
{
	float time = 0.0f;
	float r = resolution;
	double bpm = 120;
	long tickLast = 0L;
	for (int i = 0; i < tempos.size(); )
	{
		TempoEvent* e = &(tempos[i]);
		if (tick > e->tick)
		{
			time = (float)(time + frequency * 60 / bpm * ((float)(e->tick - tickLast) / r));
			bpm = e->bpm;
			tickLast = e->tick;
			i++;
		}
		break;
	}
	return (int)(time + frequency * 60 / bpm * ((float)(tick - tickLast) / r));
}

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