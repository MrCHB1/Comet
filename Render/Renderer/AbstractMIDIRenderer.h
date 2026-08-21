#pragma once

#include <vector>
#include <memory>
#include <chrono>
#include "../../MIDI/MIDISequence.h"
#include "../RenderEngine/Buffers.h" // needed for getting Framebuffer
#include "../ColorAsset.h"
#include "../NoteCounter/NoteCounterInfo.h"
#include "Config/MIDIPlayerConfig.h"
#include "QuadDrawer.h"

#define LOAD_VAL(n, key, target) \
        if (n && n[key]) { target = n[key].as<std::decay_t<decltype(target)>>(); }

class AbstractMIDIRenderer;
class MIDIApp;

static std::vector<std::shared_ptr<AbstractMIDIRenderer>> RENDERERS = { };

class AbstractMIDIRenderer
{
protected:
	std::shared_ptr<MIDISequence> seq;
	std::unique_ptr<Framebuffer> sceneFramebuffer;
	int width = 0;
	int height = 0;
	bool isTimeBased = false;

	std::shared_ptr<NoteCounterInfo> noteCounterInfo;
	std::array<bool, MIDI_KEYS> blackArr{};

	MIDIApp* app;
	ColorAsset colors;

	// Fullscreen quad for special effects
	std::unique_ptr<Quad> fullscreenQuad;
public:
	AbstractMIDIRenderer(MIDIApp* app) : app(app)
	{
		for (size_t key = 0; key < MIDI_KEYS; key++)
		{
			size_t k = (key % 12);
			blackArr[key] = k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
		}
	}
	virtual ~AbstractMIDIRenderer() = default;

	virtual ColorAsset& GetColorAsset() { return colors; }
	virtual void LoadColors(const std::vector<std::array<float, 3>>& colors)
	{
		if (colors.empty())
		{
			this->colors.ResetColors();
			this->colors.LoadColors();
		}
		else
		{
			this->colors.LoadColors(colors, true);
		}
	}

	virtual void LoadSequence(std::shared_ptr<MIDISequence> sequence)
	{
		seq = sequence;
		isTimeBased = seq->timeBased;
		noteCounterInfo->ppq = sequence->resolution;
	}

	virtual std::shared_ptr<MIDISequence> GetSequence()
	{
		return seq;
	}

	virtual void UnloadSequence()
	{
		seq.reset();
		noteCounterInfo->ResetCounter();
	}

	virtual void Initialize();
	virtual void Render(double deltaTime) = 0;
	virtual void RenderSettings();
	virtual void ResetSettings() {}
	virtual void OnResize(int newWidth, int newHeight)
	{
		width = newWidth;
		height = newHeight;
		if (sceneFramebuffer)
		{
			sceneFramebuffer->Resize(width, height);
		}
	}
	// Usually called when switching between realtime playback and rendering.
	virtual void ResetRenderer() {}

	virtual GLuint GetSceneTexture()
	{
		return sceneFramebuffer ? sceneFramebuffer->GetSceneTexture() : 0;
	}

	virtual void SetNoteCounter(std::shared_ptr<NoteCounterInfo> noteCounterInfo)
	{
		this->noteCounterInfo = noteCounterInfo;
	}

	virtual std::string GetSerializationKey() const = 0;
	virtual YAML::Node GetSettings() { return YAML::Node(); }
	virtual void LoadSettings(const YAML::Node& node) {}
};