#include "UI/MenuBuilder.h"
#include "UI/Dialog.h"
#include "UI/NavigationBar.h"
#include "MIDIApp.h"
#include <GLFW/glfw3.h>

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
	const char* title = "Window";
	bool fullscreen = false;
};