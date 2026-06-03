#include "MainWindow.h"
#include "Diagnosis/DiagnosisDialog.h"
#include "Dialog/LoadingDialog.h"
#include "Dialog/RenderVideoDialog.h"
#include "Dialog/SettingsDialog.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "Render/Renderer/PrimitiveShaders.h"

#include <iostream>
#include "nfd.h"
#include "../Inter.hpp"
#include "Comet.h"

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
	midiApp = std::make_unique<MIDIApp>();
}

void MainWindow::InitializeAppResources()
{
#ifdef COMET_DEBUG
	std::cout << "Loading app resources" << std::endl;
#endif
	InitPrimitiveShaders();
	midiApp->LoadResources();
	midiApp->SetWindow(window);
#ifdef COMET_DEBUG
	std::cout << "App resources loaded" << std::endl;
#endif
}

void MainWindow::PostInit()
{

}

void MainWindow::Run()
{
	while (!glfwWindowShouldClose(window))
	{
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

void MainWindow::DetectKeyPress()
{
	// ignore key presses if any dialog is opened
	if (dialogManager.IsAnyDialogOpened()) return;

	ImGuiIO& io = ImGui::GetIO();
	for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++)
	{
		if (!ImGui::IsKeyPressed((ImGuiKey)key)) continue;

		ImGuiKey pressedKey = (ImGuiKey)key;
		midiApp->RegisterKeyPress(pressedKey);
	}
}

void MainWindow::RenderUI()
{
	menuBuilder.Draw();
}

void MainWindow::InitializeTheme()
{
	ImGuiIO& io = ImGui::GetIO();

	ImFontConfig cfg;
	cfg.FontDataOwnedByAtlas = false;
	ImFont* font = io.Fonts->AddFontFromMemoryTTF((void*)Inter_18pt_Regular_ttf_data, Inter_18pt_Regular_ttf_size, 18.0f, &cfg);
	io.FontDefault = font;

	ImGuiStyle& style = ImGui::GetStyle();

	// -----------------------
	// Layout / feel
	// -----------------------
	style.WindowRounding = 6.0f;
	style.ChildRounding = 4.0f;
	style.FrameRounding = 4.0f;
	style.PopupRounding = 4.0f;
	style.ScrollbarRounding = 6.0f;
	style.GrabRounding = 4.0f;

	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;

	style.ItemSpacing = ImVec2(8, 6);
	style.WindowPadding = ImVec2(10, 10);
	style.WindowMenuButtonPosition = ImGuiDir_None;
	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

	// -----------------------
	// Colors (Unity Light vibe)
	// -----------------------
	ImVec4* c = style.Colors;

	c[ImGuiCol_TitleBg] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
	c[ImGuiCol_TitleBgActive] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
	c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);

	c[ImGuiCol_TabUnfocused] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
	c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);

	c[ImGuiCol_DockingPreview] = ImVec4(0.20f, 0.45f, 0.90f, 0.25f);
	c[ImGuiCol_DockingEmptyBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);

	c[ImGuiCol_Text] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

	c[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
	c[ImGuiCol_ChildBg] = ImVec4(0.97f, 0.97f, 0.97f, 1.00f);
	c[ImGuiCol_PopupBg] = ImVec4(0.97f, 0.97f, 0.97f, 1.00f);

	c[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
	c[ImGuiCol_Separator] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);

	// Frames (inputs, panels)
	c[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	c[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
	c[ImGuiCol_FrameBgActive] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);

	// Buttons (very subtle like Unity)
	c[ImGuiCol_Button] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
	c[ImGuiCol_ButtonHovered] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
	c[ImGuiCol_ButtonActive] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);

	// Headers (tree nodes, collapsibles)
	c[ImGuiCol_Header] = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
	c[ImGuiCol_HeaderHovered] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
	c[ImGuiCol_HeaderActive] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);

	// Tabs
	c[ImGuiCol_Tab] = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
	c[ImGuiCol_TabHovered] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
	c[ImGuiCol_TabSelected] = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
	c[ImGuiCol_TabSelectedOverline] = ImVec4(0.30f, 0.50f, 0.90f, 1.00f);

	// Scrollbar
	c[ImGuiCol_ScrollbarBg] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
	c[ImGuiCol_ScrollbarGrab] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
	c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

	// Accent (Unity-like blue selection)
	c[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.45f, 0.90f, 1.00f);
	c[ImGuiCol_SliderGrab] = ImVec4(0.20f, 0.45f, 0.90f, 1.00f);
	c[ImGuiCol_SliderGrabActive] = ImVec4(0.15f, 0.40f, 0.85f, 1.00f);

	c[ImGuiCol_ResizeGrip] = ImVec4(0.80f, 0.80f, 0.80f, 0.30f);
	c[ImGuiCol_ResizeGripHovered] = ImVec4(0.20f, 0.45f, 0.90f, 0.70f);
	c[ImGuiCol_ResizeGripActive] = ImVec4(0.20f, 0.45f, 0.90f, 1.00f);

	// Main menu
	c[ImGuiCol_MenuBarBg] = ImVec4(0.92f, 0.92f, 0.92f, 1.0f);

	// Plot (optional but nice)
	c[ImGuiCol_PlotLines] = ImVec4(0.20f, 0.45f, 0.90f, 1.00f);
	c[ImGuiCol_PlotHistogram] = ImVec4(0.20f, 0.45f, 0.90f, 1.00f);
}