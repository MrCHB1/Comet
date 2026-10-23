#include "UI/MenuBuilder.h"
#include "UI/Dialog.h"
#include "UI/NavigationBar.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <string>

class MIDIApp;
class UpdateChecker;

struct WindowRect
{
	int x, y;
	int width, height;
};

struct InputState
{
	// mouse
	int mouseButton = -1;
	bool mouseDown = false;
	bool mouseHold = false;
	bool mouseUp = false;
	
	// keyboard
	ImGuiKey key = ImGuiKey_None;
	bool ctrl = false;
	bool shift = false;
	bool alt = false;
	bool keyDown = false;
	bool keyHold = false;
	bool keyUp = false;
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

	bool IsGLReady() const
	{
		return isGLReady;
	}
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
	void DetectInput();
	void RenderUI();

	std::unique_ptr<MIDIApp> midiApp;
	std::unique_ptr<UpdateChecker> updateChecker;

	MenuBuilder menuBuilder;
	DialogManager dialogManager;
	GLFWwindow* window = nullptr;
	WindowRect lastWindowRect;
	std::string title = "Window";
	std::string titleInfo = "No MIDI Loaded";
	bool fullscreen = false;
	bool isGLReady = false;
};