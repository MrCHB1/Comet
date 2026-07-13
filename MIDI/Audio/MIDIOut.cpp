#include "MIDIOut.h"
#include <iostream>
#include "Utils.h"

#ifdef WIN32

typedef void (*KDMAPI_Init)();
typedef void (*KDMAPI_Terminate)();
typedef void (*KDMAPI_Send)(uint32_t msg);

HMODULE omni = LoadLibraryA("OmniMIDI.dll");

auto Init = (KDMAPI_Init)GetProcAddress(omni, "InitializeKDMAPIStream");
auto Send = (KDMAPI_Send)GetProcAddress(omni, "SendDirectData");
auto End = (KDMAPI_Terminate)GetProcAddress(omni, "TerminateKDMAPIStream");

MIDIOut::MIDIOut()
{
	std::cout << "Loading MIDI out" << std::endl;
	if (!omni)
	{
		Utils::Dialogs::ShowDialog("KDMAPI warning", "OmniMIDI is not installed or the driver has not been registered. As Comet depends on OmniMIDI for audio, it will continue without it.", Utils::Dialogs::DialogType::DIALOG_WARNING);
	}
	if (Init) Init();
}

MIDIOut::~MIDIOut()
{
	if (End) End();
}

void MIDIOut::SendEvent(uint32_t msg)
{
	if (Send) Send(msg);
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