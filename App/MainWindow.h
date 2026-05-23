#include "UI/MenuBuilder.h"
#include "UI/Dialog.h"
#include "MIDIApp.h"
#include <GLFW/glfw3.h>

class MainWindow
{
public:
	MainWindow(const char* title);
	~MainWindow();
	void Run();
private:
	// called before glfw initialization
	void InitializeApp();
	// called after glfw/glad initialization so resources can be loaded into the gpu
	void InitializeAppResources();

	bool InitializeGLFW();
	void InitializeUI();
	void InitializeDialogs();
	void InitializeTheme();

	std::unique_ptr<MIDIApp> midiApp;

	MenuBuilder menuBuilder;
	DialogManager dialogManager;
	GLFWwindow* window = nullptr;
	const char* title = "Window";
};