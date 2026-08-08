#pragma once
#include <array>
#include <memory>
#include <yaml-cpp/yaml.h>

#include "MIDIOut.h"
#include "MIDI/MIDISequence.h"
#include "MIDI/Timer/MIDITimer.h"

class MIDIAudio;

// base class for different audio engines such as realtime audio (midi out) and prerendered audio
class AudioEngine
{
	friend class MIDIAudio;
public:
	virtual ~AudioEngine() = default;
	virtual void Initialize() = 0;
	virtual void Destroy() = 0;

	virtual std::string GetName() { return "Audio"; }
	virtual std::string GetSerializationKey() { return "audio"; }
	virtual bool IsSupported() { return true; }

	virtual YAML::Node GetSettings() { return YAML::Node(); }
	virtual void LoadSettings(const YAML::Node& node) {}

	virtual void Start(std::shared_ptr<MIDISequence> seq, std::shared_ptr<MIDITimer> timer) = 0;
	virtual void Stop() = 0;
	virtual void Reset() = 0;
	virtual void Mute() = 0;
	virtual void Unmute() = 0;

	virtual bool IsPlaying() = 0;

	// displays the engine's settings. kinda weird to define the method here but makes stuff easier ig
	virtual void RenderSettings() { }
protected:
	std::shared_ptr<MIDISequence> seq;
	std::shared_ptr<MIDITimer> timer;
};

enum class AudioEngineType
{
	Realtime,
	Prerendered,
	Count
};

#define AUDIO_ENGINE_INDEX(name) static_cast<size_t>(AudioEngineType::name)
#define AUDIO_ENGINE_INDEX_VAR(var) static_cast<size_t>(var)

typedef std::array<std::unique_ptr<AudioEngine>, AUDIO_ENGINE_INDEX(Count)> AudioEngineList;

class MIDIAudio
{
public:
	MIDIAudio();

	void Start(std::shared_ptr<MIDISequence> seq, std::shared_ptr<MIDITimer> timer)
	{
		AudioEngine* currentEngine = GetCurrentEngine();
		currentEngine->seq = seq;
		currentEngine->timer = timer;
		currentEngine->Start(seq, timer);
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

	const AudioEngineType GetCurrentEngineType() const
	{
		return currentEngine;
	}

	void SwitchEngine(AudioEngineType engine);
	AudioEngineList& GetEngineList();

	YAML::Node GetSettings();
	void LoadSettings(const YAML::Node& node);
private:
	std::shared_ptr<MIDIOut> midiOut;
	AudioEngineType currentEngine = AudioEngineType::Realtime;

	AudioEngineList audioEngines{ nullptr };
};