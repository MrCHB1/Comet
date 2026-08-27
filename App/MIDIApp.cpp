#include "MIDIApp.h"
#include "MainWindow.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include "../MIDI/AbstractMIDILoader.h"
#include <thread>
#include <memory>
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "FFmpeg/FFmpegCommandBuilder.h"
#include "Utils.h"
#include "../Render/MIDIRendererPFA.h"
#include "../Render/MIDIRendererSynthesia.h"
#include "../Render/MIDIRendererEnhanced.h"
#include "../Render/MIDIRendererChannels.h"
#include "../Render/MIDIRendererMIDITrail.h"
#include "../Render/MIDIRendererVelocities.h"
#include "../Render/MIDIRendererCounter.h"
#include "../Render/MIDIRenderer.h" // Textured renderer + AbstractMIDIRenderer full definition

#include "../ResourcePack/DefaultResourcePack.h"
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
#include "FFmpeg/FFmpegPipe.h"
#include "MIDI/Audio/MIDIAudio.h"
#include "MIDI/Audio/PrerenderEngine/PrerenderedEngine.h"
#include "Models.h"
#include "UI/Themes/Themes.h"
#include "FontList.h"

MIDIApp::MIDIApp(MainWindow* mainWindow)
{
	config = MIDIPlayerConfig{};
	config.LoadConfigOrDefault();

	renderView = std::make_shared<RenderView>();
	timer = std::make_shared<MIDITimer>();
	navigationBar = std::make_unique<NavigationBar>(timer, renderView.get());

	noteCounterInfo = std::make_shared<NoteCounterInfo>();
	noteCounterRenderer = std::make_unique<NoteCounterRenderer>(noteCounterInfo, this);
	Models::LoadModels();

	this->mainWindow = mainWindow;
}

MIDIApp::~MIDIApp()
{
	if (midiAudio)
	{
		config.audioSettings = midiAudio->GetSettings();
	}
	if (renderer)
	{
		config.renderersSettings[renderer->GetSerializationKey()] = renderer->GetSettings();
	}
	config.SaveConfig();
	Models::UnloadModels();
}

std::shared_ptr<AbstractMIDILoader> MIDIApp::CreateLoader(const char* path)
{
	std::shared_ptr<AbstractMIDILoader> loader;
	if (config.midi.multithreadedLoading)
		loader = std::make_shared<MultithreadedMIDILoader>(path);
	else
		loader = std::make_shared<MIDILoader>(path);

	loader->SetLoadOnlyNotes(config.midi.loadNotesOnly);
	return loader;
}

void MIDIApp::LoadMIDI(const char* path)
{
	std::lock_guard<std::mutex> lock(appMutex);

	if (loading) return;
	UnloadMIDI();
	loading.store(true);
	std::shared_ptr<AbstractMIDILoader> loader = CreateLoader(path);
	prog = loader;

	std::thread([this, loader, pathStr = std::string(path)]() {
		std::shared_ptr<MIDISequence> seq;
		try
		{
			auto* config = this->GetConfig();
			long startMs = static_cast<long>(Utils::GetCurrTime<std::chrono::milliseconds>());
			seq = loader->Load(config->midi.timeBasedLoading);
			long endMs = static_cast<long>(Utils::GetCurrTime<std::chrono::milliseconds>());

			std::cout << "Loaded in " << (endMs - startMs) << "ms" << std::endl;
		}
		catch (const std::runtime_error& e)
		{
			std::cout << "An error occured while loading the MIDI.\n" << e.what() << std::endl;
			this->loading.store(false);
			return;
		}

		bool hasStopped = this->prog->hasStopped.load();

		if (this->prog == loader)
			this->prog = nullptr;

		if (!hasStopped && seq)
		{
			this->pendingTitle = std::filesystem::path(pathStr).filename().string();
			this->pendingSeq = seq;
			this->hasPendingSeq.store(true);
		}
		
		this->loading.store(false);
	}).detach();
	return;
}

void MIDIApp::UnloadMIDI()
{
	if (!hasSequence) return;
	timer->Pause();
	midiAudio->Reset();
	renderer->UnloadSequence();
	hasSequence = false;
	mainWindow->SetTitleInfo();
}

void MIDIApp::UpdatePendingSequence()
{
	if (!hasPendingSeq.exchange(false)) return;

	std::shared_ptr<MIDISequence> seq = pendingSeq;
	pendingSeq.reset();

	renderer->LoadSequence(seq);
	navigationBar->SetMIDILengthFromSeq(*seq.get());
	noteCounterInfo->ppq.value = seq->resolution;
	hasSequence = true;
	seqLength = seq->CalcLengthMilliseconds() / 1000.0;

	midiAudio->Start(seq, this->timer);
	timer->ClearFlags();
	timer->Start(-3.0);
	mainWindow->SetTitleInfo(pendingTitle);
}

template <typename T>
void MIDIApp::SetRenderer()
{
	static_assert(std::is_base_of_v<AbstractMIDIRenderer, T>, "T must derive from AbstractMIDIRenderer");

	int width = config.render.GetWidth();
	int height = config.render.GetHeight();
	noteCounterRenderer->OnResize(width, height); // hacky but oh well

	// save current renderer settings before destroying it
	if (this->renderer)
	{
		std::string oldKey = this->renderer->GetSerializationKey();
		config.renderersSettings[oldKey] = this->renderer->GetSettings();
	}

	// get renderer's sequence so the new one can automatically load it
	std::shared_ptr<MIDISequence> seq;
	if (this->renderer) seq = GetRenderer()->GetSequence();

	this->renderer = std::make_unique<T>(this);
	this->renderer->OnResize(width, height);
	this->renderer->SetNoteCounter(noteCounterInfo);
	this->renderer->Initialize();

	// load new renderer settings IF they exist in config storage
	std::string newKey = this->renderer->GetSerializationKey();
	if (config.renderersSettings[newKey])
	{
		this->renderer->LoadSettings(config.renderersSettings[newKey]);
	}

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

template void MIDIApp::SetRenderer<MIDIRendererPFA>();
template void MIDIApp::SetRenderer<MIDIRendererSynthesia>();
template void MIDIApp::SetRenderer<MIDIRenderer>();
template void MIDIApp::SetRenderer<MIDIRendererEnhanced>();
template void MIDIApp::SetRenderer<MIDIRendererMIDITrail>();
template void MIDIApp::SetRenderer<MIDIRendererChannels>();
template void MIDIApp::SetRenderer<MIDIRendererVelocities>();
template void MIDIApp::SetRenderer<MIDIRendererCounter>(); // <--- errors here

// called after glfw/glad initialization has finished, and is safe to load stuff, such as images, for rendering
void MIDIApp::LoadResources()
{
	themesList = std::make_unique<ThemesList>("./assets/themes");
	themesList->SetThemeAndApply(config.app.currThemeID);

	auto defaultPack = DefaultResourcePack::Instance();
	defaultPack->Init();

	// load color palettes
	LoadColorPalettes();

	// load resource packs
	packList = std::make_unique<ResourcePackList>();
	packList->RefreshList();

	int width = config.render.GetWidth();
	int height = config.render.GetHeight();

#ifdef COMET_DEBUG
	std::cout << std::endl << "[MIDIApp] Initializing render engine...\n" << std::endl;
#endif
	RendererType currRenderer = config.render.GetCurrentRenderer();

	switch (currRenderer)
	{
	case RendererType::PFA:
		SetRenderer<MIDIRendererPFA>();
		break;
	case RendererType::Synthesia:
		SetRenderer<MIDIRendererSynthesia>();
		break;
	case RendererType::Textured:
		SetRenderer<MIDIRenderer>();
		break;
	case RendererType::Enhanced:
		SetRenderer<MIDIRendererEnhanced>();
		break;
	case RendererType::MIDITrail:
		SetRenderer<MIDIRendererMIDITrail>();
		break;
	case RendererType::Channels:
		SetRenderer<MIDIRendererChannels>();
		break;
	case RendererType::Velocities:
		SetRenderer<MIDIRendererVelocities>();
		break;
	case RendererType::CounterOnly:
		SetRenderer<MIDIRendererCounter>();
		break;
	default:
		SetRenderer<MIDIRendererPFA>();
		break;
	}

	blurredQuadRenderer = std::make_unique<BlurredQuadRenderer>();
	blurredQuadRenderer->SetSceneTexture(renderer->GetSceneTexture());

	renderFramebuffer = std::make_unique<Framebuffer>();
	renderFramebuffer->Setup(width, height);
	fullscreenQuad = std::make_unique<Quad>();
	fullscreenQuad->SetShader(SCENE_SHADER);
	fullscreenQuad->SetTransform({ glm::vec3(0.0f), glm::vec2(1.0f) }, false);
#ifdef COMET_DEBUG
	std::cout << std::endl << "[MIDIApp] Render engine initialized\n" << std::endl;
#endif

	// load audio stuff
	midiAudio = std::make_unique<MIDIAudio>();
	midiAudio->LoadSettings(config.audioSettings);
	midiAudio->GetCurrentEngine()->Initialize();
}

void MIDIApp::BuildFontAtlas()
{
	fontList = std::make_unique<FontList>();

	auto& io = ImGui::GetIO();
	io.Fonts->AddFontDefault();

	fontList->RegisterFontsIntoAtlas(io.Fonts, 18.0f);
}

void MIDIApp::LoadColorPalettes()
{
	if (colorList == nullptr)
		colorList = std::make_shared<ColorPaletteList>();
}

void MIDIApp::Update()
{
	UpdatePendingSequence();

	ImVec4 bgColor = config.render.GetBackground();
	renderFramebuffer->Bind();

	int width = renderFramebuffer->GetWidth();
	int height = renderFramebuffer->GetHeight();

	glViewport(0, 0, width, height);

	float bgAlpha = (rendering && currentRenderSettings.renderTransparencyMask) ? 0.0f : 1.0f;
	glClearColor(bgColor.x, bgColor.y, bgColor.z, bgAlpha);
	glClear(GL_COLOR_BUFFER_BIT);

	if (rendering)
	{
		renderer->Render(1.0 / (double)currentRenderSettings.fps);
	}
	else
	{
		double time = Utils::GetCurrTime<std::chrono::microseconds>() / 1000000.0;
		renderer->Render(time - lastFrameTime);
		UpdateNoteCounterInfo();
		lastFrameTime = time;
	}

	// make sure the render framebuffer is bound
	renderFramebuffer->Bind();
	{
		{
			ShaderBind blurBind(*BLUR_SHADER);
			BLUR_SHADER->SetFloat("width", width);
			BLUR_SHADER->SetFloat("height", height);
			BLUR_SHADER->SetInt("scene", 0);
		}

		{
			ShaderBind sceneBind(*SCENE_SHADER);
			SCENE_SHADER->SetInt("scene", 0);
		}

		TextureBind sceneTextureBind(renderer->GetSceneTexture(), 0);
		fullscreenQuad->Draw();
	}

	if (config.render.showCounter)
	{
		glm::vec2 counterResolution = noteCounterRenderer->GetCounterResolution();
		float heightOffset = rendering || !mainWindow->CanShowNavigationBar() ? 0.0 : 56.0;
		glm::vec2 counterPos = noteCounterRenderer->GetCounterPosition();

		// the (now optional) frosted glass effect, yay!
		if (config.overlayInfo.blurBehind)
			blurredQuadRenderer->Render({ glm::vec3(counterPos.x, counterPos.y, 0.0f), glm::vec2(counterResolution.x, counterResolution.y) });

		noteCounterRenderer->Render(heightOffset);
	}

	renderFramebuffer->Unbind();

	// render what was in the framebuffer
	{
		int winW, winH;
		GLFWwindow* window = mainWindow->GetInternalWindow();
		glfwGetFramebufferSize(window, &winW, &winH);
		glViewport(0, 0, winW, winH);

		{
			ShaderBind sceneBind(*SCENE_SHADER);
			SCENE_SHADER->SetInt("scene", 0);
		}

		TextureBind sceneTextureBind(renderFramebuffer->GetSceneTexture(), 0);
		fullscreenQuad->Draw();
	}

	if (!rendering)
	{
		if (mainWindow->CanShowNavigationBar()) navigationBar->Draw();

		if (hasSequence && timer->Elapsed() >= seqLength + 3.0 && !timer->IsPaused())
		{
			timer->Pause();
		}
	}
}

void MIDIApp::UpdateNoteCounterInfo()
{
	double time = Utils::GetCurrTime<std::chrono::microseconds>() / 1000000.0;
	noteCounterInfo->fps = 1.0 / (time - lastFrameTime);
	noteCounterInfo->audioBuffer = -1.0;
	auto* prAudio = dynamic_cast<PrerenderedEngine*>(GetMIDIAudio()->GetCurrentEngine());
	if (prAudio)
	{
		noteCounterInfo->audioBuffer = prAudio->GetBufferSeconds();
	}
}

void MIDIApp::RegisterKeyPress(ImGuiKey key, bool ctrl, bool shift, bool alt)
{
	MIDIPlayerConfig* config = GetConfig();

	switch (key)
	{
	case ImGuiKey_Space:
	{
		if (IsRendering()) return;
		// TODO: ignore when no sequence is loaded
		timer->TogglePause();
		break;
	}
	case ImGuiKey_LeftArrow:
	{
		if (IsRendering()) return;
		double currTime = timer->Elapsed();
		double seekTime = std::max(-3.0, currTime - config->navigation.seekBackwardSeconds);
		timer->NavigateTo(seekTime);
		break;
	}
	case ImGuiKey_RightArrow:
	{
		if (IsRendering()) return;
		double currTime = timer->Elapsed();
		double seekTime = std::min(seqLength + 5.0, currTime + config->navigation.seekForwardSeconds);
		timer->NavigateTo(seekTime);
		break;
	}
	case ImGuiKey_Enter:
	{
		if (!alt) return;
		mainWindow->ToggleFullscreen();
		break;
	}
	default:
		break;
	}
}

void MIDIApp::OnResize(int width, int height)
{
	if (rendering) return;

	config.render.SetWidth(width);
	config.render.SetHeight(height);

	if (!renderer)
	{
		std::cout << "Renderer uninitialized!" << std::endl;
		return;
	}
	renderer->OnResize(width, height);
	noteCounterRenderer->OnResize(width, height);
	renderFramebuffer->Resize(width, height);
}

void MIDIApp::RunFrame()
{
	if (rendering)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			rendering.store(false);
		}

		double midiTime = (double)currentFrame / (double)currentRenderSettings.fps - currentRenderSettings.midiStartDelay;
		this->timer->NavigateTo(midiTime);

		Update();

		if (!this->rendering || midiTime >= seqLength + 5.0)
		{
			std::cout << "Rendering complete or stopped!" << std::endl;
			ffmpegPipe->Close();
			this->FinalizeRendering();
			this->timer->NavigateTo(lastSavedTimeSecs);
			return;
		}
	}
	else
	{
		Update();
	}
}

void MIDIApp::CaptureFrame()
{
	if (!this->rendering) return;

	int width = renderFramebuffer->GetWidth(),
		height = renderFramebuffer->GetHeight();

	renderFramebuffer->Bind();
	glViewport(0, 0, width, height);
	if (currentFrame >= 0)
	{
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glReadPixels(0, 0, width, height,
			GL_RGBA, GL_UNSIGNED_BYTE, exportPixels.data());
		ffmpegPipe->Write(exportPixels.data(), exportPixels.size());
	}
	renderFramebuffer->Unbind();

	// draw back onto the screen
	{
		int winW, winH;
		GLFWwindow* window = mainWindow->GetInternalWindow();
		glfwGetFramebufferSize(window, &winW, &winH);
		glViewport(0, 0, winW, winH);

		ShaderBind sceneBind(*SCENE_SHADER);
		SCENE_SHADER->SetInt("scene", 0);
		TextureBind sceneTextureBind(renderFramebuffer->GetSceneTexture(), 0);
		fullscreenQuad->Draw();
	}

	currentFrame++;
	if (currentFrame % 10 == 0) std::cout << "Rendered frame " << currentFrame << std::endl;
}

// Will render to a video until cancelled
void MIDIApp::RenderMIDIVideo(const RenderSettings& renderSettings)
{
	this->lastSavedTimeSecs = this->timer->Elapsed();
	this->currentRenderSettings = renderSettings;

	this->PrepareRendering();
	this->currentFrame = -1; // -1 to account for renderers that might need an extra frame to fully settle

	this->exportPixels.resize(renderSettings.width * renderSettings.height * 4);

	// setup FFmpeg
	std::string cmd = FFmpegCommandBuilder::BuildFFmpegCommand(renderSettings);
	std::cout << "Running FFmpeg command: " << cmd << std::endl;
	ffmpegPipe = std::make_unique<FFmpegPipe>();
	if (!ffmpegPipe->Open(cmd))
	{
		std::cout << "Failed to open FFmpeg." << std::endl;
		this->FinalizeRendering();
		return;
	}
}

void MIDIApp::PrepareRendering()
{
	std::lock_guard<std::mutex> lock(renderMtx);
	this->rendering = true;

	int previewWidth = 0;
	int previewHeight = 0;
	GLFWwindow* window = mainWindow->GetInternalWindow();
	glfwGetWindowSize(window, &previewWidth, &previewHeight);

	renderer->OnResize(currentRenderSettings.width, currentRenderSettings.height);
	noteCounterRenderer->OnResize(currentRenderSettings.width, currentRenderSettings.height);
	renderFramebuffer->Resize(currentRenderSettings.width, currentRenderSettings.height);

	lastRenderStartTimeMs = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
	if (!timer->IsPaused()) timer->Pause();
	midiAudio->Mute();
	GetRenderer()->ResetRenderer();
}

void MIDIApp::FinalizeRendering()
{
	std::lock_guard<std::mutex> lock(renderMtx);
	this->rendering = false;

	int winW, winH;
	GLFWwindow* window = mainWindow->GetInternalWindow();
	glfwGetFramebufferSize(window, &winW, &winH);
	config.render.SetWidth(winW);
	config.render.SetHeight(winH);

	renderer->OnResize(winW, winH);
	noteCounterRenderer->OnResize(winW, winH);
	renderFramebuffer->Resize(winW, winH);

	double endRenderTime = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
	std::cout << "Rendered MIDI in " << Utils::FormatDuration2(endRenderTime - lastRenderStartTimeMs) << "!" << std::endl;

	midiAudio->Unmute();
	GetRenderer()->ResetRenderer();
}