#include "MIDIApp.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include "../MIDI/AbstractMIDILoader.h"
#include <thread>
#include <memory>
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "Utils.h"
MIDIApp::MIDIApp()
{
	config = MIDIPlayerConfig{};
	renderView = std::make_shared<RenderView>();
	timer = std::make_shared<MIDITimer>();
	navigationBar = std::make_unique<NavigationBar>(timer, renderView.get());

	noteCounterInfo = std::make_shared<NoteCounterInfo>();
	noteCounterRenderer = std::make_unique<NoteCounterRenderer>(noteCounterInfo);
}

void MIDIApp::LoadMIDI(const char* path)
{
	std::lock_guard<std::mutex> lock(appMutex);

	if (loading) return;
	UnloadMIDI();
	loading.store(true);
	std::shared_ptr<AbstractMIDILoader> loader = CreateLoader(path);
	prog = loader;

	std::thread([this, loader]() {
		std::shared_ptr<MIDISequence> seq;
		try
		{
			long startMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			seq = loader->Load();
			long endMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

			std::cout << "Loaded in " << (endMs - startMs) << "ms" << std::endl;
		}
		catch (const std::exception& e)
		{
			std::cout << "An error occured while loading the MIDI.\n" << e.what() << std::endl;
			return;
		}
		this->loading.store(false);
		
		if (this->prog == loader)
			this->prog = nullptr;

		if (seq)
		{
			this->timer->Start(-3.0);
			this->renderer->LoadSequence(seq);
			this->navigationBar->SetMIDILengthFromSeq(*seq.get());
			this->noteCounterInfo->ppq.value = seq->resolution;
			this->hasSequence = true;
			this->seqLength = seq->CalcLengthMilliseconds() / 1000.0;
		}
	}).detach();
	return;
}

void MIDIApp::UnloadMIDI()
{
	if (!hasSequence) return;
	renderer->UnloadSequence();
	hasSequence = false;
}

// called after glfw/glad initialization has finished, and is safe to load stuff, such as images, for rendering
void MIDIApp::LoadResources()
{
	auto defaultPack = &DefaultResourcePack::Instance();
	defaultPack->Init();

	int width = config.render.GetWidth();
	int height = config.render.GetHeight();

	renderer = std::make_unique<MIDIRenderer>(this);
	renderer->LoadResourcePack(defaultPack);
	renderer->OnResize(width, height);
	renderer->SetNoteCounter(noteCounterInfo);
	noteCounterRenderer->OnResize(width, height);
#ifdef COMET_DEBUG
	std::cout << std::endl << "[MIDIApp] Initializing render engine...\n" << std::endl;
#endif
	renderer->Initialize();
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
}

void MIDIApp::Update()
{
	ImVec4 bgColor = config.render.GetBackground();
	renderFramebuffer->Bind();

	int width = renderFramebuffer->GetWidth();
	int height = renderFramebuffer->GetHeight();

	glViewport(0, 0, width, height);
	glClearColor(bgColor.x, bgColor.y, bgColor.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	renderer->Render();

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
		float heightOffset = rendering ? 0.0 : 56.0;
		glm::vec2 counterPos = noteCounterRenderer->GetCounterPosition();
		// the frosted glass effect, yay!
		blurredQuadRenderer->Render({ glm::vec3(counterPos.x, counterPos.y, 0.0f), glm::vec2(counterResolution.x, counterResolution.y) });
		noteCounterRenderer->Render(heightOffset);
	}

	renderFramebuffer->Unbind();

	// render what was in the framebuffer
	{
		int winW, winH;
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
		navigationBar->Draw();
		if (hasSequence && timer->Elapsed() >= seqLength + 3.0 && !timer->IsPaused())
		{
			timer->Pause();
		}
	}
}

void MIDIApp::RegisterKeyPress(ImGuiKey key)
{
	switch (key)
	{
		case ImGuiKey_Space:
		{
			// TODO: ignore when no sequence is loaded
			timer->TogglePause();
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
	// if (renderer.get()) renderer->OnResize(width, height);
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
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glReadPixels(0, 0, width, height,
		GL_RGBA, GL_UNSIGNED_BYTE, exportPixels.data());
	ffmpegPipe->Write(exportPixels.data(), exportPixels.size());
	renderFramebuffer->Unbind();

	// draw back onto the screen
	{
		int winW, winH;
		glfwGetFramebufferSize(window, &winW, &winH);
		glViewport(0, 0, winW, winH);

		ShaderBind sceneBind(*SCENE_SHADER);
		SCENE_SHADER->SetInt("scene", 0);
		TextureBind sceneTextureBind(renderFramebuffer->GetSceneTexture(), 0);
		fullscreenQuad->Draw();
	}

	currentFrame++;
	std::cout << "Rendered frame " << currentFrame << std::endl;
}

// Will render to a video until cancelled
void MIDIApp::RenderMIDIVideo(const RenderSettings& renderSettings)
{
	this->lastSavedTimeSecs = this->timer->Elapsed();
	this->currentRenderSettings = renderSettings;

	this->PrepareRendering(); // Sets this->rendering = true
	this->currentFrame = 0;

	this->exportPixels.resize(renderSettings.width * renderSettings.height * 4);

	// setup FFmpeg
	std::string cmd = BuildFFmpegCommand(renderSettings);
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
	glfwGetWindowSize(window, &previewWidth, &previewHeight);

	renderer->OnResize(currentRenderSettings.width, currentRenderSettings.height);
	noteCounterRenderer->OnResize(currentRenderSettings.width, currentRenderSettings.height);
	renderFramebuffer->Resize(currentRenderSettings.width, currentRenderSettings.height);

	lastRenderStartTimeMs = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
	if (!timer->IsPaused()) timer->Pause();
}

void MIDIApp::FinalizeRendering()
{
	std::lock_guard<std::mutex> lock(renderMtx);
	this->rendering = false;

	int winW, winH;
	glfwGetFramebufferSize(window, &winW, &winH);
	config.render.SetWidth(winW);
	config.render.SetHeight(winH);

	renderer->OnResize(winW, winH);
	noteCounterRenderer->OnResize(winW, winH);
	renderFramebuffer->Resize(winW, winH);

	double endRenderTime = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
	std::cout << "Rendered MIDI in " << Utils::FormatDuration2(endRenderTime - lastRenderStartTimeMs) << "!" << std::endl;
}