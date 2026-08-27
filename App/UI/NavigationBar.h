#pragma once

#include "MIDI/Timer/MIDITimer.h"
#include "MIDI/MIDISequence.h"
#include "MIDI/TempoMap.h"
#include "Render/RenderView.h"
#include "Render/GPUImage.h"
#include <memory>

#include <glm/vec2.hpp>

class MIDIApp;

class NavigationBar
{
public:
	NavigationBar(MIDIApp* app, RenderView* renderView);

	void Draw();
	void Update();
	glm::vec2 GetResolution() const
	{
		return lastResolution;
	}
	glm::vec2 GetPosition() const
	{
		return lastPosition;
	}

	void SetMIDILengthFromSeq(const MIDISequence& seq)
	{
		midiLength = seq.tempoMap->TicksToSecsFromMap(seq.resolution, seq.length) + 3.0;
	}
private:
	MIDIApp* app;
	std::unique_ptr<GPUImage> uiTexture;
	RenderView* renderView;

	bool texturesLoaded = false;
	double midiLength = 1.0;
	// float lastHeight = 0.0f;
	glm::vec2 lastResolution;
	glm::vec2 lastPosition;

	void TryLoadUITextures();
	void VerticalSeparator(float height = 0.0f);
};