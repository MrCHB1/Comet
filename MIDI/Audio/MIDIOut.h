#pragma once
#ifdef WIN32
#include <cstdint>

#include <vector>
#include <Windows.h>

class MIDIOut
{
public:
	MIDIOut();
	~MIDIOut();
	void SendEvent(unsigned int msg);
private:

};

#else

class MIDIOut
{
public:
    MIDIOut() {}
    void SendEvent(unsigned int msg)
	{
		// nothing here for other platforms unfortunately :(
	}
private:

};
#endif
