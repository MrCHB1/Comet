#pragma once

#include <cstdint>

#ifdef _WIN32

#include <vector>
#include <Windows.h>

typedef void (*KDMAPI_Init)();
typedef void (*KDMAPI_Terminate)();
typedef void (*KDMAPI_Send)(uint32_t msg);

class MIDIOut
{
public:
	MIDIOut() = default;
	~MIDIOut();
	void Initialize();
	void Shutdown();
	void SendEvent(uint32_t msg);
private:
	HMODULE omni = nullptr;

	KDMAPI_Init InitStream = nullptr;
	KDMAPI_Send Send = nullptr;
	KDMAPI_Terminate End = nullptr;

	bool initialized = false;
	bool attempted = false;
};

#else

class MIDIOut
{
public:
	MIDIOut() = default;
	~MIDIOut();
	void Initialize();
	void Shutdown();
	void SendEvent(uint32_t msg);
private:
	void* handle = nullptr;
	bool attempted = false;
	void (*sendDirectData)(uint32_t msg) = nullptr;
	void (*terminateStream)() = nullptr;
};
#endif