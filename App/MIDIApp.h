#pragma once
#include "../Config/MIDIPlayerConfig.h"
#include "../ResourcePack/DefaultResourcePack.h"
#include "../Render/MIDIRenderer.h"
#include "../ResourcePack/ResourcePackList.h"
#include "ColorPalette/ColorPaletteList.h"
#include "MIDI/MIDILoader.h"
#include "MIDI/MultithreadedMIDILoader.h"
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
#include "Models.h"
#include "UI/Themes/Themes.h"

class MainWindow;

class MIDIApp
{
public:
	MIDIApp(MainWindow* mainWindow);
	~MIDIApp()
	{
		config.SaveConfig();
		Models::UnloadModels();
	}

	AbstractMIDIRenderer* GetRenderer()
	{
		return renderer.get();
	}

	template <typename T>
	void SetRenderer()
	{
		static_assert(std::is_base_of_v<AbstractMIDIRenderer, T>, "T must derive from AbstractMIDIRenderer");

		int width = config.render.GetWidth();
		int height = config.render.GetHeight();
		noteCounterRenderer->OnResize(width, height); // hacky but oh well

		// get renderer's sequence so the new one can automatically load it
		std::shared_ptr<MIDISequence> seq;
		if (this->renderer) seq = GetRenderer()->GetSequence();

		this->renderer = std::make_unique<T>(this);
		this->renderer->OnResize(width, height);
		this->renderer->SetNoteCounter(noteCounterInfo);
		this->renderer->Initialize();

		if (seq != nullptr) this->renderer->LoadSequence(seq);

		if (blurredQuadRenderer)
			blurredQuadRenderer->SetSceneTexture(this->renderer->GetSceneTexture());

		// ensure the colors are properly loaded if using images for colors
		auto* colorList = GetColorList();
		if (config.render.GetUseColorsFromImage())
		{
			colorList->SetPalette(config.render.paletteID);
			auto& palette = colorList->GetCurrentPalette();
			this->renderer->GetColorAsset().LoadColors(palette.palette, config.render.loopColors);
		}
	}

	void LoadResources();

	void LoadColorPalettes();
	void LoadMIDI(const char* path);
	void UnloadMIDI();
	void RenderMIDIVideo(const RenderSettings& renderSettings);
	void RegisterKeyPress(ImGuiKey key, bool ctrl, bool shift, bool alt);

	ThemesList* GetThemeList()
	{
		return themesList.get();
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
		std::shared_ptr<AbstractMIDILoader> loader;
		if (config.midi.multithreadedLoading)
			loader = std::make_shared<MultithreadedMIDILoader>(path);
		else
			loader = std::make_shared<MIDILoader>(path);

		loader->SetLoadOnlyNotes(config.midi.loadNotesOnly);
		return loader;
	}

	bool hasSequence = false;
	std::atomic_bool rendering = false;
	double seqLength = 0.0;
private:
	MainWindow* mainWindow;
	MIDIPlayerConfig config;

	std::unique_ptr<ThemesList> themesList;
	std::unique_ptr<AbstractMIDIRenderer> renderer;
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
	double lastFrameTime = 0;
	RenderSettings currentRenderSettings;
	int currentFrame = 0;
	std::vector<uint8_t> exportPixels{};
	std::unique_ptr<FFmpegPipe> ffmpegPipe;

	// prepares the app for rendering (disabling navigation, ui, etc.)
	void PrepareRendering();
	// finalizes rendering (re-enables navigation, etc.)
	void FinalizeRendering();
};