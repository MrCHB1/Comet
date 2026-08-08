#pragma once

#include "../MIDIAudio.h"
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>

#if defined(WIN32)

#define SAMPLE_RATE 48000
#define BUFFER_SECS_MIN 1.0
#define BUFFER_SECS_MAX 300.0

#include "BASSMIDI.h"
#include <bass.h>

struct PrecalculatedEvent
{
    uint32_t tick;
    uint32_t message;
};

class PrerenderedEngine : public AudioEngine
{
public:
    PrerenderedEngine();
    ~PrerenderedEngine() override;

    void Initialize() override;
    void Destroy() override;
    std::string GetName() override { return "Prerendered Audio"; }

    void Start(std::shared_ptr<MIDISequence> seq, std::shared_ptr<MIDITimer> timer) override;
    void Stop() override;
    void Reset() override;
    void Mute() override;
    void Unmute() override;

    bool IsPlaying() override;

    // UI for soundfonts and voices
    void RenderSettings() override;

private:
    int GetSkippingVelocity()
    {
        std::shared_ptr<MIDITimer> currentTimer;
        {
            std::lock_guard<std::mutex> lock(timerMutex);
            currentTimer = timer;
        }
        if (currentTimer && currentTimer->IsPaused()) return 0;

        std::lock_guard<std::mutex> lock(bufferMutex);
        int diff = 127 + 10 - (bufferWritePos - bufferReadPos) / 100;
        if (diff > 127) diff = 127;
        if (diff < 0) diff = 0;
        return diff;
    }

    void EventGeneratorLoop();
    void RenderLoop(double startTime);
    // checks for timer pauses/navigation on a 10ms interval and adjusts the prerenderer as necessary (i think it's a weird way to do this lol)
    void TimerLoop();

    void LoadSoundfonts(std::vector<std::wstring> paths);

    void SyncPlayer(double time);
    void KillLastGenerator();
    void StartPrerender(bool force, double time);

    static DWORD CALLBACK PlaybackStreamProc(HSTREAM handle, void* buffer, DWORD length, void* user);
    void BassWriteWrapped(int start, int count);
    void ResetEvents();

    double GetPlayerTime()
    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        return startTime + bufferReadPos / 48000.0;
    }

    double GetBufferSeconds()
    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        return std::max(0, bufferWritePos - bufferReadPos) / 48000.0;
    }
    void WrappedCopy(float* src, int pos, int srcCount, float* dst, int pos2, int count);
    void ResizeBuffer(double newBufferLengthSecs);

    std::unique_ptr<BASSMIDI> bassMidi;
    HSTREAM playbackStream = 0;

    std::thread generatorThread;
    std::thread renderThread;
    std::thread timerCheckThread;

    std::atomic_bool stopFlag{ false };
    std::atomic_bool awaitingReset{ false };
    std::atomic_bool timerCheckStop{ false };
    std::atomic_bool isPlaying{ false };
    std::atomic_bool muted{ false };

    double startTime = 0.0;

    // shared Event State
    std::vector<PrecalculatedEvent> prerenderedEvents;
    std::atomic_bool eventsReady{ false };
    std::atomic<size_t> eventCursor{ 0 };
    std::mutex eventMutex;

    // circular audio buffer
    float* audioBuffer = nullptr;
    int bufferLength = 0;
    int bufferWritePos = 0;
    int bufferReadPos = 0;
    std::mutex bufferMutex;
    std::mutex timerMutex;

    // engine settings
    int maxVoices = 1000;  
    bool noFx = false;     
    std::vector<std::string> soundfontPaths{};
    int selectedSoundfontIndex = -1;
    double bufferSeconds = 5.0;

    double fps = 0.0;        
    double instability = 1.0;

    // debug
    std::atomic<int> overrunCount{ 0 };
    std::atomic<int> maxWaitTimeMs{ 0 };

    // limiter (LoudMax) Settings
    bool reduceHighPitch = false;
    double loudnessL = 1.0;
    double loudnessR = 1.0;
    double velocityR = 0.0;
    double velocityL = 0.0;
    double strength = 1.0;
    double minThresh = 0.4;
    double velocityThresh = 1.0;
    double attackRate = 1.0;   
    double releaseRate = 0.005;
    std::atomic<double> preVolume{ 1.0 };

    std::shared_ptr<MIDISequence> lastSeq;
};
#else
class PrerenderedEngine : public AudioEngine
{
public:
    PrerenderedEngine() {}
    ~PrerenderedEngine() override {}

    void Initialize() override {}
    void Destroy() override {}
    std::string GetName() override { return "Prerendered Audio (Unsupported)"; }

    void Start(std::shared_ptr<MIDISequence> seq, std::shared_ptr<MIDITimer> timer) override {}
    void Stop() override {}
    void Reset() override {}
    void Mute() override {}
    void Unmute() override {}

    bool IsPlaying() override
    {
        return false;
    }

    // UI for soundfonts and voices
    void RenderSettings() override {}
};
#endif