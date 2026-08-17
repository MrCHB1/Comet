#pragma once

#include "MIDI/Timer/MIDITimer.h"
#include "MIDI/MIDISequence.h"
#include "MIDI/TempoMap.h"
#include "Render/RenderView.h"
#include <memory>

class NavigationBar
{
public:
	NavigationBar() = default;
	NavigationBar(std::shared_ptr<MIDITimer> timer, RenderView* renderView) : timer(timer), renderView(renderView) {}

	void Draw();
	void Update();

	void SetMIDILengthFromSeq(const MIDISequence& seq)
	{
		midiLength = seq.tempoMap->TicksToSecsFromMap(seq.resolution, seq.length) + 3.0;
	}
private:
	// included so we can nagivate around the midi by only using this timer
	std::shared_ptr<MIDITimer> timer;
	RenderView* renderView;

	double midiLength = 1.0;

};