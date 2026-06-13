#pragma once

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
	~MIDIOut() = default;
	void SendEvent(uint32_t msg)
	{
		// nothing here for other platforms unfortunately :(
	}
private:

};
#endif