#pragma once

#include "../Config/MIDIPlayerConfig.h"
#include "VideoRender/RenderSettings.h"

#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <cstdint>
#include <type_traits>

class MainWindow;
class ThemesList;
class FontList;
class AbstractMIDIRenderer;
class NavigationBar;
class NoteCounterInfo;
class NoteCounterRenderer;
class BlurredQuadRenderer;
class ResourcePackList;
class ColorPaletteList;
class RenderView;
class Progress;
class MIDITimer;
class MIDIAudio;
class Framebuffer;
class Quad;
class FFmpegPipe;
class AbstractMIDILoader;
class MIDISequence;

// Matches Dear ImGui's `enum ImGuiKey : int { ... };` declaration exactly
// (name, underlying type, non-class enum) so we can use it here without
// pulling in the rest of imgui.h.
enum ImGuiKey : int;

class MIDIApp
{
public:
	MIDIApp(MainWindow* mainWindow);
	~MIDIApp();

	AbstractMIDIRenderer* GetRenderer()
	{
		return renderer.get(); // unique_ptr<T>::get() is fine with incomplete T
	}

	template <typename T>
	void SetRenderer();

	void LoadResources();

	void LoadColorPalettes();
	void LoadMIDI(const char* path);
	void UnloadMIDI();
	void UpdatePendingSequence();
	void RenderMIDIVideo(const RenderSettings& renderSettings);
	void RegisterKeyPress(ImGuiKey key, bool ctrl, bool shift, bool alt);

	MIDIAudio* GetMIDIAudio() { return midiAudio.get(); }
	ThemesList* GetThemeList() { return themesList.get(); }
	FontList* GetFontList() { return fontList.get(); }
	NoteCounterInfo* GetNoteCounterInfo() { return noteCounterInfo.get(); }
	NoteCounterRenderer* GetNoteCounterRenderer() { return noteCounterRenderer.get(); }
	MIDIPlayerConfig* GetConfig() { return &config; }
	RenderView* GetRenderView() { return renderView.get(); }
	std::shared_ptr<MIDITimer> GetTimer() { return timer; }
	std::shared_ptr<Progress> GetProgress() { return prog; }
	ResourcePackList* GetPackList() { return packList.get(); }
	ColorPaletteList* GetColorList() { return colorList.get(); }

	const RenderSettings& GetCurrentRenderSettings() const { return currentRenderSettings; }

	bool IsLoading() const { return loading.load(); }
	bool IsRendering() const { return rendering.load(); }

	void BuildFontAtlas();

	void Update();
	void RunFrame();
	void CaptureFrame();
	void OnResize(int width, int height);

	// Body lives in MIDIApp.cpp: constructs MIDILoader/MultithreadedMIDILoader
	// via make_shared, which needs their complete types.
	std::shared_ptr<AbstractMIDILoader> CreateLoader(const char* path);

	bool hasSequence = false;
	std::atomic_bool rendering = false;
	double seqLength = 0.0;
private:
	MainWindow* mainWindow;
	MIDIPlayerConfig config;

	std::unique_ptr<ThemesList> themesList;
	std::unique_ptr<FontList> fontList;
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
	std::unique_ptr<MIDIAudio> midiAudio;
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

	// staging variables for thread-safe loading
	std::shared_ptr<MIDISequence> pendingSeq = nullptr;
	std::string pendingTitle = "";
	std::atomic_bool hasPendingSeq{ false };

	// prepares the app for rendering (disabling navigation, ui, etc.)
	void PrepareRendering();
	// finalizes rendering (re-enables navigation, etc.)
	void FinalizeRendering();
};