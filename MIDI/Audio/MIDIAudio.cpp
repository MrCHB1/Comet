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

	AudioEngine* oldEngine = GetCurrentEngine();
	std::shared_ptr<MIDISequence> seq = oldEngine->seq;
	std::shared_ptr<MIDITimer> timer = oldEngine->timer;

	bool lastPlaying = oldEngine->IsPlaying();

	oldEngine->Destroy();
	currentEngine = engine;

	AudioEngine* newEngine = GetCurrentEngine();
	newEngine->Initialize();

	if (lastPlaying)
	{
		newEngine->Start(seq, timer);
	}
}

AudioEngineList& MIDIAudio::GetEngineList()
{
	return audioEngines;
}