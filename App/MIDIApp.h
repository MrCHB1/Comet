#pragma once
#include "../Config/MIDIPlayerConfig.h"
#include "../ResourcePack/DefaultResourcePack.h"
#include "../Render/MIDIRenderer.h"
#include "../MIDI/MIDILoader.h"
#include <memory>
#include <mutex>
#include <atomic>

class MIDIApp
{
public:
	MIDIApp();
	MIDIRenderer* GetRenderer()
	{
		return renderer.get();
	}
	void LoadResources();
	void LoadMIDI(const char* path);

	MIDIPlayerConfig* GetConfig()
	{
		return &config;
	}

	std::shared_ptr<Progress> GetProgress()
	{
		return prog;
	}

	bool IsLoading()
	{
		return loading.load();
	}

	void Update();
	void OnResize(int width, int height);

	std::shared_ptr<AbstractMIDILoader> CreateLoader(const char* path)
	{
		std::shared_ptr<AbstractMIDILoader> loader = std::make_shared<MIDILoader>(path);
		loader->SetLoadOnlyNotes(config.midi.loadNotesOnly);
		return loader;
	}
private:
	MIDIPlayerConfig config;

	std::unique_ptr<MIDIRenderer> renderer;
	std::shared_ptr<Progress> prog;
	std::atomic_bool loading = false;

	std::mutex appMutex;
	std::mutex thisMtx;
};