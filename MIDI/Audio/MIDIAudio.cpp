#include "MIDIAudio.h"
#include "AudioThread.h"

MIDIAudio::MIDIAudio()
{
	midiOut = std::make_shared<MIDIOut>();

	audioEngines[AUDIO_ENGINE_INDEX(Realtime)] = std::make_unique<AudioThread>(midiOut);
}

void MIDIAudio::SwitchEngine(AudioEngineType engine)
{
	if (engine == currentEngine || engine == AudioEngineType::Count)
		return;

	GetCurrentEngine()->Destroy();
	currentEngine = engine;
	GetCurrentEngine()->Initialize();
}