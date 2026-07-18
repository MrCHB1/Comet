#include "MainWindow.h"
#include "Diagnosis/DiagnosisDialog.h"
#include "Dialog/LoadingDialog.h"
#include "Dialog/RenderVideoDialog.h"
#include "Dialog/SettingsDialog.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "Render/Renderer/PrimitiveShaders.h"
#include "Fonts.h"
#include <iostream>
#include "nfd.h"
#include "Comet.h"
#include "UI/Themes/Themes.h"

#include "icon32.hpp"

MainWindow::MainWindow(const char* title)
{
	this->title = title;
	InitializeApp();
	InitializeDialogs();
	InitializeUI();

	if (!InitializeGLFW())
	{
		std::cerr << "Failed to initialize GLFW for main window" << std::endl;
		return;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	CometDefaultThemes::InitializeDefaultThemes();
	InitializeTheme();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	InitializeAppResources();

#ifdef COMET_DEBUG
	std::cout << "GL version: " << glGetString(GL_VERSION) << std::endl;
#endif

	PostInit();
}

MainWindow::~MainWindow()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	if (window)
	{
		glfwDestroyWindow(window);
	}
	glfwTerminate();
}

void MainWindow::InitializeDialogs()
{
	dialogManager.RegisterDialog<DiagnosisDialog>(midiApp.get());
	dialogManager.RegisterDialog<LoadingDialog>(midiApp.get());
	dialogManager.RegisterDialog<RenderVideoDialog>(midiApp.get());
	dialogManager.RegisterDialog<SettingsDialog>(midiApp.get());
}

void MainWindow::InitializeUI()
{
	SubMenu& fileMenu = menuBuilder.CreateMenu("File");
	fileMenu.AddItem(new MenuButton("Open...", [this]() {
		std::string outPath;
		Utils::ChooseFile(outPath, "mid");
		if (outPath.empty()) return;
		this->midiApp->LoadMIDI(outPath.c_str());
		this->dialogManager.GetDialog<LoadingDialog>()->Open();
	}));
	fileMenu.AddItem(new MenuButton("Render to Video...", [this]() {
		this->dialogManager.GetDialog<RenderVideoDialog>()->Open();
	}));
	// fileMenu.AddItem(new MenuButton("Play without Render..."));
	fileMenu.AddItem(new MenuButton("Diagnose MIDI...", [this]() {
		this->dialogManager.GetDialog<DiagnosisDialog>()->Open();
	}));
	fileMenu.AddItem(new MenuButton("Take Screenshot", [this]() {
		std::cout << "Will be implemented soon" << std::endl;
	}));
	fileMenu.AddItem(new MenuButton("Quit", [this]() { glfwSetWindowShouldClose(window, GLFW_TRUE); }));

	SubMenu& playMenu = menuBuilder.CreateMenu("Play");
	playMenu.AddItem(new MenuButton("Stop", [this]() {
		auto timer = midiApp->GetTimer();
		timer->Stop();
	}));
	playMenu.AddItem(new MenuButton("Stop and Unload", [this]() {
		auto timer = midiApp->GetTimer();
		timer->Stop();
		midiApp->UnloadMIDI();
	}));

	SubMenu& viewMenu = menuBuilder.CreateMenu("View");
	viewMenu.AddItem(new MenuCheckbox("Show note counter", &(midiApp->GetConfig()->render.showCounter)));

	SubMenu& optionMenu = menuBuilder.CreateMenu("Options");
	optionMenu.AddItem(new MenuButton("Settings...", [this]() {
		this->dialogManager.GetDialog<SettingsDialog>()->Open();
	}));
	SubMenu& helpMenu = menuBuilder.CreateMenu("Help");
	helpMenu.AddItem(new MenuButton("Buy ponluxime a coffee", [this]() {
		Utils::OpenURL("https://ko-fi.com/ponluxime");
	}));
}

bool MainWindow::InitializeGLFW()
{
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GL_FALSE);

	MIDIPlayerConfig* cfg = midiApp->GetConfig();
	int width = cfg->render.GetWidth(), height = cfg->render.GetHeight();

	window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	if (!window)
	{
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return false;
	}

	glfwSetWindowUserPointer(window, midiApp.get());
	glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int w, int h)
		{
			glViewport(0, 0, w, h);
			MIDIApp* app = (MIDIApp*)glfwGetWindowUserPointer(window);
			if (app)
			{
				app->OnResize(w, h);
			}
		});

	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	return true;
}

void MainWindow::InitializeApp()
{
	midiApp = std::make_unique<MIDIApp>(this);
}

void MainWindow::InitializeAppResources()
{
#ifdef COMET_DEBUG
	std::cout << "Loading app resources" << std::endl;
#endif
	LoadWindowIcon();
	InitPrimitiveShaders();
	midiApp->LoadResources();
#ifdef COMET_DEBUG
	std::cout << "App resources loaded" << std::endl;
#endif
}

void MainWindow::LoadWindowIcon()
{
	GLFWimage image;
	image.pixels = stbi_load_from_memory(
		icon32_png_data,
		static_cast<int>(icon32_png_size),
		&image.width,
		&image.height,
		nullptr,
		4
	);

	if (image.pixels)
	{
		glfwSetWindowIcon(window, 1, &image);
		stbi_image_free(image.pixels);
	}
}

void MainWindow::PostInit()
{
	glfwSwapInterval(midiApp->GetConfig()->render.GetVSync() ? 1 : 0);
}

void MainWindow::Run()
{
	while (!glfwWindowShouldClose(window))
	{
		double frameStartTime = glfwGetTime();

		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();

		if (!midiApp->IsRendering())
		{
			ImGui::NewFrame();
			DetectKeyPress();

			midiApp->RunFrame();
			RenderUI();
			dialogManager.DrawDialogs();

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			MIDIPlayerConfig* config = midiApp->GetConfig();
			if (config->render.GetFPSLimit() >= 15)
			{
				double targetFrameTime = 1.0 / config->render.GetFPSLimit();
				double elapsedFrameTime = glfwGetTime() - frameStartTime;

				if (elapsedFrameTime < targetFrameTime)
				{
					double timeToSleep = targetFrameTime - elapsedFrameTime;
					std::this_thread::sleep_for(std::chrono::duration<double>(timeToSleep));
				}
			}
		}
		else
		{
			ImGuiIO& io = ImGui::GetIO();
			const auto& settings = midiApp->GetCurrentRenderSettings();
			io.DisplaySize = ImVec2((float)settings.width, (float)settings.height);

			ImGui::NewFrame();
			midiApp->RunFrame();
			if (!midiApp->IsRendering())
			{
				ImGui::EndFrame();
				continue;
			}

			ImGui::Render();
			// bake note counter into frame, etc.
			midiApp->CaptureFrame();

			// now its safe to draw dialogs
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			dialogManager.DrawDialogs(); 

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		glfwSwapBuffers(window);
	}
}

bool MainWindow::CanShowNavigationBar()
{
	MIDIPlayerConfig* config = midiApp->GetConfig();

	if (!fullscreen && !config->navigation.alwaysHideBar) return true;

	double mouseY;
	glfwGetCursorPos(window, nullptr, &mouseY);

	// me
	bool isMenuOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
	if (mouseY < 60.0 || isMenuOpen) return true;

	return false;
}

void MainWindow::DetectKeyPress()
{
	// ignore key presses if any dialog is opened
	if (dialogManager.IsAnyDialogOpened()) return;

	ImGuiIO& io = ImGui::GetIO();
	for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++)
	{
		if (!ImGui::IsKeyPressed((ImGuiKey)key)) continue;

		bool ctrl = io.KeyCtrl;
		bool shift = io.KeyShift;
		bool alt = io.KeyAlt;

		ImGuiKey pressedKey = (ImGuiKey)key;
		midiApp->RegisterKeyPress(pressedKey, ctrl, shift, alt);
	}
}

void MainWindow::ToggleFullscreen()
{
	if (!fullscreen)
	{
		glfwGetWindowPos(window, &lastWindowRect.x, &lastWindowRect.y);
		glfwGetWindowSize(window, &lastWindowRect.width, &lastWindowRect.height);

		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);

		glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
		glfwSetWindowPos(window, 0, 0);
		glfwSetWindowSize(window, mode->width, mode->height);
	}
	else
	{
		glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
		glfwSetWindowPos(window, lastWindowRect.x, lastWindowRect.y);
		glfwSetWindowSize(window, lastWindowRect.width, lastWindowRect.height);
	}

	fullscreen = !fullscreen;
}

void MainWindow::RenderUI()
{
	if (CanShowNavigationBar()) menuBuilder.Draw();
}

void MainWindow::InitializeTheme()
{
	Fonts::LoadFonts();
	// CometDefaultThemes::DefaultLightMode->ApplyTheme();
}
