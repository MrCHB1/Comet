#pragma once

#include <cstdint>

#ifdef WIN32

#include <vector>
#include <Windows.h>

class MIDIOut
{
public:
	MIDIOut();
	~MIDIOut();
	void SendEvent(uint32_t msg);
private:

};

#else

class MIDIOut
{
public:
	MIDIOut();
	~MIDIOut();
	void SendEvent(uint32_t msg);

private:
	void* handle = nullptr;
	void (*sendDirectData)(uint32_t msg) = nullptr;
	void (*terminateStream)() = nullptr;
};
#endif