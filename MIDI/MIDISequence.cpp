#include "TempoMap.h"
#include "MIDISequence.h"

MIDISequence::MIDISequence() : MIDISequence("Unnamed") {}
MIDISequence::MIDISequence(const char* name)
	: name(name), tracks({}), tempos({}), timeSignatures({}) {}

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

MIDISequence::~MIDISequence() = default;

TempoMap* MIDISequence::GetTempoMap()
{
	return tempoMap.get();
}