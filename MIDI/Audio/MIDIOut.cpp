#include "MIDIOut.h"
#include <iostream>
#include "Utils.h"

#ifdef WIN32

MIDIOut::MIDIOut()
{
	std::cout << "Loading MIDI out" << std::endl;

	omni = LoadLibraryA("OmniMIDI.dll");
	if (!omni)
	{
		Utils::Dialogs::ShowDialog(
			"KDMAPI warning",
			"OmniMIDI is not installed or the driver has not been registered. "
			"As Comet depends on OmniMIDI for audio, it will continue without it.",
			Utils::Dialogs::DialogType::DIALOG_WARNING
		);
		return;
	}

	Init = (KDMAPI_Init)GetProcAddress(omni, "InitializeKDMAPIStream");
	Send = (KDMAPI_Send)GetProcAddress(omni, "SendDirectData");
	End = (KDMAPI_Terminate)GetProcAddress(omni, "TerminateKDMAPIStream");

	if (!Init || !Send || !End)
	{
		Utils::Dialogs::ShowDialog(
			"KDMAPI warning",
			"OmniMIDI was found, but one or more required KDMAPI functions could not be loaded.",
			Utils::Dialogs::DialogType::DIALOG_WARNING
		);

		FreeLibrary(omni);
		omni = nullptr;
		Init = nullptr;
		Send = nullptr;
		End = nullptr;
		return;
	}

	if (Init) Init();
	initialized = true;
}

MIDIOut::~MIDIOut()
{
	if (initialized && End)
		End();

	if (omni) FreeLibrary(omni);
}

void MIDIOut::SendEvent(uint32_t msg)
{
	if (initialized && Send) Send(msg);
}

#else

#include <dlfcn.h>
#include <unistd.h>
#include <climits>
#include <string>

typedef void (*KDMAPI_Init)();
typedef void (*KDMAPI_Terminate)();
typedef void (*KDMAPI_Send)(uint32_t msg);

static void* LoadOmniMIDI()
{
	// try the standard library search path before fallback
	void* handle = dlopen("libOmniMIDI.so", RTLD_NOW | RTLD_LOCAL);
	if (handle)
		return handle;

	// fall back to current directory
	char buffer[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX - 1);
	if (count > 0)
	{
		std::string path(buffer, count);
		size_t slash = path.find_last_of('/');
		if (slash != std::string::npos)
		{
			path = path.substr(0, slash + 1) + "libOmniMIDI.so";
			handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
		}
	}
	return handle;
}

MIDIOut::MIDIOut()
{
	std::cout << "Loading MIDI out" << std::endl;
	handle = LoadOmniMIDI();
	if (!handle)
	{
		std::cerr << "Failed to load libOmniMIDI.so: " << dlerror() << std::endl;
		return;
	}

	auto init = (KDMAPI_Init)dlsym(handle, "InitializeKDMAPIStream");
	sendDirectData = (KDMAPI_Send)dlsym(handle, "SendDirectData");
	terminateStream = (KDMAPI_Terminate)dlsym(handle, "TerminateKDMAPIStream");

	if (!init || !sendDirectData || !terminateStream)
	{
		std::cerr << "libOmniMIDI.so is missing KDMAPI symbols" << std::endl;
		dlclose(handle);
		handle = nullptr;
		sendDirectData = nullptr;
		terminateStream = nullptr;
		return;
	}

	init();
}

MIDIOut::~MIDIOut()
{
	if (terminateStream)
		terminateStream();
	if (handle)
		dlclose(handle);
}

void MIDIOut::SendEvent(uint32_t msg)
{
	if (sendDirectData)
		sendDirectData(msg);
}
#endif