#include "UI/MenuBuilder.h"
#include "UI/Dialog.h"
#include "UI/NavigationBar.h"
#include "MIDIApp.h"
#include <GLFW/glfw3.h>
#include <string>

struct WindowRect
{
	int x, y;
	int width, height;
};

class MainWindow
{
public:
	MainWindow(const char* title);
	~MainWindow();
	void Run();
	bool CanShowNavigationBar();
	bool IsFullscreen() { return fullscreen; }
	void ToggleFullscreen();
	void SetTitleInfo(std::string midiName = "");
	GLFWwindow* GetInternalWindow() { return window; }
private:
	// called before glfw initialization
	void InitializeApp();
	// called after glfw/glad initialization so resources can be loaded into the gpu
	void InitializeAppResources();
	bool InitializeGLFW();
	void InitializeUI();
	void InitializeDialogs();
	void InitializeTheme();
	void LoadWindowIcon();
	void PostInit();
	void DetectKeyPress();
	void RenderUI();

	std::unique_ptr<MIDIApp> midiApp;

	MenuBuilder menuBuilder;
	DialogManager dialogManager;
	GLFWwindow* window = nullptr;
	WindowRect lastWindowRect;
	std::string title = "Window";
	std::string titleInfo = "No MIDI Loaded";
	bool fullscreen = false;
};