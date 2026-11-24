#include "UI/MenuBuilder.h"
#include "UI/Dialog.h"
#include "UI/NavigationBar.h"

#include <string>

class MIDIApp;
class UpdateChecker;
struct GLFWwindow;

struct WindowLaunchOptions
{
	std::string midi = "";
};

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
	void SetLaunchOptions(WindowLaunchOptions options)
	{
		this->options = std::move(options);
	}
	void Run();
	bool CanShowNavigationBar();
	bool IsFullscreen() { return fullscreen; }
	void ToggleFullscreen();
	void SetTitleInfo(std::string midiName = "");
	GLFWwindow* GetInternalWindow() { return window; }

	bool IsGLReady() const { return isGLReady; }
	int GetGLMajor() const { return glMajor; }
	int GetGLMinor() const { return glMinor; }
	// true if the created context is at least major.minor
	bool HasGLVersion(int major, int minor) const
	{
		return glMajor > major || (glMajor == major && glMinor >= minor);
	}
private:
	// called before glfw initialization
	void InitializeApp();
	// called after glfw/glad initialization so resources can be loaded into the gpu
	void InitializeAppResources();
	bool InitializeGLFW();
	void InitializeUI();
	void BuildMenuBar();
	void InitializeDialogs();
	void InitializeTheme();
	void LoadWindowIcon();
	void PostInit();
	void DetectInput();
	void RenderUI();

	std::unique_ptr<MIDIApp> midiApp;
	std::unique_ptr<UpdateChecker> updateChecker;

	WindowLaunchOptions options{};
	MenuBuilder menuBuilder;
	DialogManager dialogManager;
	GLFWwindow* window = nullptr;
	WindowRect lastWindowRect;
	std::string title = "Window";
	std::string titleInfo = "No MIDI Loaded";
	bool fullscreen = false;
	bool isGLReady = false;
	int glMajor = 0;
	int glMinor = 0;
	std::string glslVersion;
};