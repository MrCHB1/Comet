#pragma once
#include "../Config/MIDIPlayerConfig.h"
#include "../ResourcePack/DefaultResourcePack.h"
#include "../Render/MIDIRenderer.h"
#include "../ResourcePack/ResourcePackList.h"
#include "ColorPalette/ColorPaletteList.h"
#include "../MIDI/MIDILoader.h"
#include "../MIDI/Timer/MIDITimer.h"
#include "App/UI/NavigationBar.h"
#include "../Render/RenderView.h"
#include "Render/NoteCounter/NoteCounterRenderer.h"
#include "Render/NoteCounter/NoteCounterInfo.h"
#include "Render/BlurredQuadRenderer.h"
#include "VideoRender/RenderSettings.h"
#include "FFmpeg/FFmpegPipe.h"
#include "MIDI/Audio/MIDIOut.h"
#include "MIDI/Audio/AudioThread.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

class MIDIApp
{
public:
	MIDIApp();
	MIDIRenderer* GetRenderer()
	{
		return renderer.get();
	}

	void LoadResources();

	void LoadColorPalettes();
	void LoadMIDI(const char* path);
	void UnloadMIDI();
	void RenderMIDIVideo(const RenderSettings& renderSettings);
	void RegisterKeyPress(ImGuiKey key);
	void SetWindow(GLFWwindow* window)
	{
		this->window = window;
	}

	NoteCounterInfo* GetNoteCounterInfo()
	{
		return noteCounterInfo.get();
	}

	NoteCounterRenderer* GetNoteCounterRenderer()
	{
		return noteCounterRenderer.get();
	}

	MIDIPlayerConfig* GetConfig()
	{
		return &config;
	}

	RenderView* GetRenderView()
	{
		return renderView.get();
	}

	std::shared_ptr<MIDITimer> GetTimer()
	{
		return timer;
	}

	std::shared_ptr<Progress> GetProgress()
	{
		return prog;
	}

	ResourcePackList* GetPackList()
	{
		return packList.get();
	}

	ColorPaletteList* GetColorList()
	{
		return colorList.get();
	}

	const RenderSettings& GetCurrentRenderSettings() const { return currentRenderSettings; }

	bool IsLoading() const
	{
		return loading.load();
	}

	bool IsRendering() const
	{
		return rendering.load();
	}

	void Update();
	void RunFrame();
	void CaptureFrame();
	void OnResize(int width, int height);

	std::shared_ptr<AbstractMIDILoader> CreateLoader(const char* path)
	{
		std::shared_ptr<AbstractMIDILoader> loader = std::make_shared<MIDILoader>(path);
		loader->SetLoadOnlyNotes(config.midi.loadNotesOnly);
		return loader;
	}

	bool hasSequence = false;
	std::atomic_bool rendering = false;
	double seqLength = 0.0;
private:
	GLFWwindow* window;
	MIDIPlayerConfig config;

	std::unique_ptr<MIDIRenderer> renderer;
	std::unique_ptr<NavigationBar> navigationBar;
	std::shared_ptr<NoteCounterInfo> noteCounterInfo;
	std::unique_ptr<NoteCounterRenderer> noteCounterRenderer;
	std::unique_ptr<BlurredQuadRenderer> blurredQuadRenderer; // for everything including note counter background, etc.
	std::unique_ptr<ResourcePackList> packList;
	std::shared_ptr<ColorPaletteList> colorList;
	std::shared_ptr<RenderView> renderView;
	std::shared_ptr<Progress> prog;
	std::shared_ptr<MIDITimer> timer;
	std::shared_ptr<MIDIOut> midiOut;
	std::unique_ptr<AudioThread> audioThread;
	std::atomic_bool loading = false;

	#pragma region Framebuffer for rendering
	std::unique_ptr<Framebuffer> renderFramebuffer;
	std::unique_ptr<Quad> fullscreenQuad;
	#pragma endregion

	std::mutex appMutex;
	std::mutex thisMtx;
	std::mutex renderMtx;

	double lastRenderStartTimeMs = 0;
	double lastSavedTimeSecs = 0;
	RenderSettings currentRenderSettings;
	int currentFrame = 0;
	std::vector<uint8_t> exportPixels{};
	std::unique_ptr<FFmpegPipe> ffmpegPipe;

	// prepares the app for rendering (disabling navigation, ui, etc.)
	void PrepareRendering();
	// finalizes rendering (re-enables navigation, etc.)
	void FinalizeRendering();
};