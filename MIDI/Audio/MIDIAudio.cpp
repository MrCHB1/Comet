#include "MIDIAudio.h"
#include "AudioThread.h"
#include "PrerenderEngine/PrerenderedEngine.h"
#include "PrerenderEngine/BASSMIDI.h"
#include <iostream>

MIDIAudio::MIDIAudio()
{
	midiOut = std::make_shared<MIDIOut>();
	audioEngines[AUDIO_ENGINE_INDEX(Realtime)] = std::make_unique<AudioThread>(midiOut);
	audioEngines[AUDIO_ENGINE_INDEX(Prerendered)] = std::make_unique<PrerenderedEngine>();
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
		Start(seq, timer);
	}
}

AudioEngineList& MIDIAudio::GetEngineList()
{
	return audioEngines;
}

YAML::Node MIDIAudio::GetSettings()
{
	YAML::Node node;
	node["currentEngine"] = static_cast<int>(currentEngine);
	YAML::Node enginesNode;
	for (size_t i = 0; i < audioEngines.size(); i++)
	{
		if (audioEngines[i])
		{
			enginesNode[audioEngines[i]->GetSerializationKey()] = audioEngines[i]->GetSettings();
		}
	}
	node["engines"] = enginesNode;
	return node;
}

void MIDIAudio::LoadSettings(const YAML::Node& node)
{
	if (!node) return;
	if (node["currentEngine"])
	{
		currentEngine = static_cast<AudioEngineType>(node["currentEngine"].as<int>());
	}
	if (node["engines"])
	{
		const auto& enginesNode = node["engines"];
		for (size_t i = 0; i < audioEngines.size(); ++i)
		{
			if (audioEngines[i])
			{
				std::string key = audioEngines[i]->GetSerializationKey();
				if (enginesNode[key])
				{
					audioEngines[i]->LoadSettings(enginesNode[key]);
				}
			}
		}
	}
}