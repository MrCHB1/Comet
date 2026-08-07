#pragma once
#include <array>
#include <memory>

#include "MIDIOut.h"
#include "MIDI/MIDISequence.h"
#include "MIDI/Timer/MIDITimer.h"

// base class for different audio engines such as realtime audio (midi out) and prerendered audio
class AudioEngine
{
public:
	virtual ~AudioEngine() = default;
	virtual void Initialize() = 0;
	virtual void Destroy() = 0;

	virtual void Start(std::shared_ptr<MIDISequence> seq, std::shared_ptr<MIDITimer> timer) = 0;
	
	virtual void Stop() = 0;
	virtual void Reset() = 0;
	virtual void Mute() = 0;
	virtual void Unmute() = 0;

	// displays the engine's settings. kinda weird to define the method here but makes stuff easier ig
	virtual void RenderSettings() { }
};

enum class AudioEngineType
{
	Realtime,
	Prerendered,
	Count
};

#define AUDIO_ENGINE_INDEX(name) static_cast<size_t>(AudioEngineType::name)
#define AUDIO_ENGINE_INDEX_VAR(var) static_cast<size_t>(var)

class MIDIAudio
{
public:
	MIDIAudio();

	void Start(std::shared_ptr<MIDISequence> seq, std::shared_ptr<MIDITimer> timer)
	{
		GetCurrentEngine()->Start(seq, timer);
	}

	void Stop()
	{
		GetCurrentEngine()->Stop();
	}

	void Reset()
	{
		GetCurrentEngine()->Reset();
	}

	void Mute()
	{
		GetCurrentEngine()->Mute();		
	}

	void Unmute()
	{
		GetCurrentEngine()->Unmute();
	}

	AudioEngine* GetCurrentEngine()
	{
		return audioEngines[AUDIO_ENGINE_INDEX_VAR(currentEngine)].get();
	}

	void SwitchEngine(AudioEngineType engine);
private:
	std::shared_ptr<MIDIOut> midiOut;
	AudioEngineType currentEngine = AudioEngineType::Realtime;

	std::array<std::unique_ptr<AudioEngine>, AUDIO_ENGINE_INDEX(Count)> audioEngines;
};