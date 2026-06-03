#pragma once

#include "NoteCounterInfo.h"

void NoteCounterInfo::ResetCounter()
{
	notesPassed.value = 0;
	polyphony.value = 0;
	timeSeconds.value = 0;
	notesPerSecond.value = 0;
	bpm.value = 120;
	tick.value = 0;
	ppq.value = 960;
}