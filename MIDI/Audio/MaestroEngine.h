#pragma once

#include "MIDIAudio.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>

#if defined(_WIN32)
#include <windows.h>
#define LIBRARY_NAME "maestro.dll"
#define open_library() LoadLibraryA(LIBRARY_NAME)
#define find_symbol(lib, name) ((void *)GetProcAddress((lib), (name)))
typedef HMODULE Library;
#else
#include <dlfcn.h>
#if defined(__APPLE__)
#define LIBRARY_NAME "libmaestro.dylib"
#else
#define LIBRARY_NAME "libmaestro.so"
#endif
#define open_library() dlopen(LIBRARY_NAME, RTLD_NOW | RTLD_LOCAL)
#define find_symbol(lib, name) dlsym((lib), (name))
typedef void* Library;
#endif

#define MAESTRO_NO_PROTOTYPES
#include "maestro.h"

class MIDISequence;
class MIDITimer;

class MaestroEngine : public AudioEngine
{
public:
	MaestroEngine() {}
	~MaestroEngine() { Destroy(); }

	std::string GetName() override { return "Maestro"; }

	bool IsSupported() override { return true; }
	void Initialize() override;
	void Destroy() override;

	void Start(std::shared_ptr<MIDISequence> seq, std::shared_ptr<MIDITimer> timer) override;
	void Stop() override;
	void Reset() override { Stop(); }
	void Mute() override {}
	void Unmute() override {}
	bool IsPlaying() override { return threadWorking.load(); }

	void RenderSettings() override;
private:
	MaestroCoreV1* core = nullptr;
	MaestroRealtimeV1* rt = nullptr;
	MaestroRealtime* inst = nullptr;
	MaestroAppInfo app{};

	bool hasMaestro = false;
	uint64_t maxScheduleTimeUs = 0;

	std::atomic_bool stopFlag{ false };
	std::atomic_bool threadWorking{ false };
	std::thread generatorThread;
	std::thread playerThread;

	// ---- generator -> player ring buffer (single producer, single consumer) ----
	struct SchedEvent
	{
		int64_t ns = 0;          // position on the song timeline
		uint32_t message = 0;
		uint32_t epoch = 0;      // which (re)sync this event belongs to
	};

	static constexpr size_t RING_SIZE = size_t(1) << 16;
	static constexpr size_t RING_MASK = RING_SIZE - 1;

	std::vector<SchedEvent> ring;
	alignas(64) std::atomic<size_t> head{ 0 };   // written by the generator
	alignas(64) std::atomic<size_t> tail{ 0 };   // written by the player

	// ---- (re)sync handshake: the player decides where playback starts ----
	std::mutex ctlMutex;
	uint64_t ctlEpoch = 0;       // guarded by ctlMutex
	int64_t ctlSeekNs = 0;       // guarded by ctlMutex
	std::atomic<uint64_t> epoch{ 0 };          // mirror of ctlEpoch for cheap polling
	std::atomic<uint64_t> genDoneEpoch{ 0 };   // epoch for which the generator has produced everything

	std::mutex scheduleMutex;                  // player sleeps here
	std::condition_variable scheduleCV;
	std::mutex spaceMutex;                     // generator sleeps here
	std::condition_variable spaceCV;

	void GeneratorLoop(std::shared_ptr<MIDISequence> seq);
	void PlayerLoop(std::shared_ptr<MIDITimer> timer);
	void MAESTRO_CALL OnLog(void* user, uint32_t level, const char* message);
};