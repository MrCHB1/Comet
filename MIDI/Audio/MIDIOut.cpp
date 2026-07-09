#include "MIDIOut.h"
#include <iostream>
#include <cstdint>

#ifdef WIN32

typedef void (*KDMAPI_Init)();
typedef void (*KDMAPI_Terminate)();
typedef void (*KDMAPI_Send)(uint32_t msg);

HMODULE omni = LoadLibraryA("OmniMIDI.dll");

auto Init = (KDMAPI_Init)GetProcAddress(omni, "InitializeKDMAPIStream");
auto Send = (KDMAPI_Send)GetProcAddress(omni, "SendDirectData");
auto End = (KDMAPI_Terminate)GetProcAddress(omni, "TerminateKDMAPIStream");

MIDIOut::MIDIOut() {
	std::cout << "Loading MIDI out" << std::endl;
	Init();
}

MIDIOut::~MIDIOut()
{
	End();
}

void MIDIOut::SendEvent(unsigned int msg)
{
	Send(msg);
}
#endif
