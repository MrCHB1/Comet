#include "MIDIApp.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include "../MIDI/AbstractMIDILoader.h"
#include <thread>

MIDIApp::MIDIApp()
{
	config = MIDIPlayerConfig{};
}

void MIDIApp::LoadMIDI(const char* path)
{
	std::lock_guard<std::mutex> lock(appMutex);

	if (loading) return;
	loading.store(true);
	renderer->UnloadSequence();
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
			std::cout << "MIDI should start playing from here on." << std::endl;
		}
	}).detach();
	return;
}

// called after glfw/glad initialization has finished, and is safe to load stuff, such as images, for rendering
void MIDIApp::LoadResources()
{
	auto defaultPack = &DefaultResourcePack::Instance();
	defaultPack->Init();

	renderer = std::make_unique<MIDIRenderer>();
	renderer->LoadResourcePack(defaultPack);
	renderer->OnResize(config.render.GetWidth(), config.render.GetHeight());
#ifdef COMET_DEBUG
	std::cout << std::endl << "[MIDIApp] Initializing render engine...\n" << std::endl;
#endif
	renderer->Initialize();
#ifdef COMET_DEBUG
	std::cout << std::endl << "[MIDIApp] Render engine initialized\n" << std::endl;
#endif
}

void MIDIApp::Update()
{
	ImVec4 bgColor = config.render.GetBackground();
	glClearColor(bgColor.x, bgColor.y, bgColor.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	renderer->Render();
}

void MIDIApp::OnResize(int width, int height)
{
	config.render.SetWidth(width);
	config.render.SetHeight(height);
	// if (renderer.get()) renderer->OnResize(width, height);
	if (!renderer)
	{
		std::cout << "Renderer uninitialized!" << std::endl;
		return;
	}
	renderer->OnResize(width, height);
}