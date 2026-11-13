#ifdef _WIN32

#include "PrerenderedEngine.h"
#include <imgui.h>
#include "imgui-knobs.h"
#include <iostream>
#include <codecvt>
#include <locale>
#include <algorithm>
#include "MIDI/MIDIDefs.h"
#include "MIDI/TempoMap.h"
#include "Utils.h"
#include "resource.h"
#include <Windows.h>
#include <sstream>
#include "App/Dialog/DialogMacros.h"

// helper to convert char* to wchar_t* for BASSMIDI Soundfont loading
static std::wstring ConvertToWideString(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}

// and back
static std::string ConvertFromWideString(const std::wstring& str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.to_bytes(str);
}

PrerenderedEngine::PrerenderedEngine()
{
    bufferSize = SAMPLE_RATE * 2 * bufferSeconds;
    audioBuffer = (float*)malloc(bufferSize * sizeof(float));
    memset(audioBuffer, 0, bufferSize * sizeof(float));
}

PrerenderedEngine::~PrerenderedEngine()
{
    Destroy();
    free(audioBuffer);
}

void PrerenderedEngine::Initialize()
{
    timerCheckStop.store(false);

    BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 10);
    BASS_SetConfig(BASS_CONFIG_BUFFER, 50);

    if (!BASS_Init(-1, SAMPLE_RATE, 0, 0, NULL)) {
        std::cout << "BASS_Init failed with error code: " << BASS_ErrorGetCode() << std::endl;
    }

    EnsureDefaultSoundfont();
    appliedSoundfontPaths = soundfonts.GetActiveSoundfontPaths();
    LoadSoundfonts(appliedSoundfontPaths);

    playbackStream = BASS_StreamCreate(SAMPLE_RATE, 2, BASS_SAMPLE_FLOAT, PlaybackStreamProc, this);
    if (playbackStream == 0)
    {
        int err = BASS_ErrorGetCode();
        std::cout << "Playback Stream Creation Failed! Error code: " << err << std::endl;
    }
    else
    {
        if (!BASS_ChannelPlay(playbackStream, FALSE))
        {
            std::cout << "Failed to start persistent playback stream. Err code: " << BASS_ErrorGetCode() << std::endl;
        }
    }

    timerCheckThread = std::thread(&PrerenderedEngine::TimerLoop, this);
}

void PrerenderedEngine::Destroy()
{
    Stop();
    timerCheckStop.store(true);

    if (generatorThread.joinable()) generatorThread.join();
    if (renderThread.joinable()) renderThread.join();
    if (timerCheckThread.joinable()) timerCheckThread.join();

    if (playbackStream)
    {
        BASS_ChannelStop(playbackStream);
        BASS_StreamFree(playbackStream);
        playbackStream = 0;
    }
    bassMidi->FreeSoundfonts();
    bassMidi.reset();
    BASS_Free();

    if (timerCheckThread.joinable()) timerCheckThread.join();
}

void PrerenderedEngine::SyncPlayer(double time)
{
    std::lock_guard<std::mutex> lock(bufferMutex);
    double t = startTime + bufferReadPos / (double)SAMPLE_RATE;
    double offset = time - t;
    int newPos = bufferReadPos + (int)(offset * SAMPLE_RATE);
    if (newPos < 0) newPos = 0;
    if (std::abs(bufferReadPos - newPos) / (double)SAMPLE_RATE > 0.03) bufferReadPos = newPos;
}

void PrerenderedEngine::Start(std::shared_ptr<MIDISequence> seq, std::shared_ptr<MIDITimer> timer)
{
    if (!seq || !timer) return;

    Stop();

    {
        std::lock_guard<std::mutex> lock(timerMutex);
        this->seq = seq;
        this->timer = timer;
        startTime = timer->Elapsed();
    }
    
    isPlaying = true;
    awaitingReset = false;
    firstLoad = true;
    eventCursor = 0;
    bufferWritePos = 0;
    bufferReadPos = 0;

    if (seq != lastSeq)
    {
        lastSeq = seq;
        eventsReady = false;

        stopFlag = false;
        generatorThread = std::thread(&PrerenderedEngine::EventGeneratorLoop, this);
    }
    else
    {
        eventsReady = true;
        stopFlag = false;
    }
    
    StartPrerender(true, startTime);
    firstLoad = false;
}

void PrerenderedEngine::Stop()
{
    stopFlag = true;
    if (generatorThread.joinable()) generatorThread.join();
    if (renderThread.joinable()) renderThread.join();
    isPlaying = false;
}

void PrerenderedEngine::Reset()
{
    std::lock_guard<std::mutex> lock(bufferMutex);
    memset(audioBuffer, 0, bufferSize * sizeof(float));
    bufferWritePos = 0;
    bufferReadPos = 0;
}

void PrerenderedEngine::Mute()
{
    muted = true;
    BASS_ChannelSetAttribute(playbackStream, BASS_ATTRIB_VOL, 0.0f);
}

void PrerenderedEngine::Unmute()
{
    muted = false;
    BASS_ChannelSetAttribute(playbackStream, BASS_ATTRIB_VOL, 1.0f);
}

bool PrerenderedEngine::IsPlaying()
{
    return isPlaying;
}

void PrerenderedEngine::BassWriteWrapped(int start, int count)
{
    start = (start * 2) % bufferSize;
    count *= 2;

    if (start + count > bufferSize)
    {
        bassMidi->Read(audioBuffer, start, bufferSize - start);
        count -= bufferSize - start;
        bassMidi->Read(audioBuffer, 0, count);
    }
    else
    {
        bassMidi->Read(audioBuffer, start, count);
    }
}

void PrerenderedEngine::EventGeneratorLoop()
{
    std::vector<PrecalculatedEvent> localEvents;
    localEvents.reserve(seq->notes * 2 + seq->mergedEvents.size());

    for (const auto& ev : seq->mergedEvents)
    {
        localEvents.push_back({ (uint32_t)ev.tick, ev.message });
        if (stopFlag) break;
    }

    if (stopFlag) return;

    for (int i = 0; i < MIDI_KEYS; ++i)
    {
        NoteSequence& notes = seq->mergedNotes[i];
        for (size_t j = 0; j < notes.Size(); j++)
        {
            uint32_t nTick = notes.tick[j];
            uint32_t nGate = notes.gate[j];
            uint8_t nNote = notes.GetKey(j);
            uint8_t nChannel = notes.GetChannel(j);
            uint8_t nVel = notes.GetVelocity(j);

            localEvents.push_back({
              nTick,
                0x90u | nChannel | ((uint32_t)nNote << 8) | ((uint32_t)nVel << 16)
                });

            localEvents.push_back({
                nTick + (long)nGate,
                0x80u | nChannel | ((uint32_t)nNote << 8)
                });

            if (stopFlag) break;
        }
        if (stopFlag) break;
    }

    if (stopFlag) return;

    std::stable_sort(localEvents.begin(), localEvents.end(), [](const PrecalculatedEvent& a, const PrecalculatedEvent& b) {
        return a.tick < b.tick;
        });

    {
        std::lock_guard<std::mutex> lock(eventMutex);
        prerenderedEvents = std::move(localEvents);
    }

    eventsReady = true;
}

void PrerenderedEngine::RenderLoop(double initialStartTime)
{
    // upper bound on samples synthesized in a single BassWriteWrapped() call.
    // keeps each unlocked synth burst short (~20ms @ 48kHz) so stopFlag is
    // re-checked often (fast response to Stop()/seeks) and so the render
    // thread never monopolizes bufferMutex for long stretches even when a
    // heavy/dense passage would otherwise justify a much bigger single write.
    constexpr int kMaxRenderChunkSamples = 960;

    while (!eventsReady && !stopFlag)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (stopFlag) return;

    bassMidi = std::make_unique<BASSMIDI>(maxVoices, noFx);
    auto* tempoMap = seq->GetTempoMap();

    {
        std::shared_ptr<MIDITimer> currentTimer;
        {
            std::lock_guard<std::mutex> lock(timerMutex);
            currentTimer = timer;
        }

        if (currentTimer)
        {
            initialStartTime = currentTimer->Elapsed();
        }
    }

    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        bufferWritePos = 0;
        bufferReadPos = 0;
        startTime = initialStartTime;
    }

    for (const auto& e : prerenderedEvents)
    {
        if (stopFlag) break;

        double evTime = seq->timeBased ? (double)e.tick / TIME_BASED_MULTIPLIER : tempoMap->TicksToSecsFromMap(seq->resolution, e.tick);
        if (fps > 0.0)
        {
            evTime = std::round(evTime * fps) / fps;
        }

        bool isNoteEvent = ((e.message & 0xF0) == 0x90 || (e.message & 0xF0) == 0x80);
        if (isNoteEvent && evTime < startTime)
        {
            continue;
        }

        if (bufferWritePos < bufferReadPos)
        {
            bufferWritePos = bufferReadPos;
        }

        double offset = evTime - startTime;
        int targetSamplePos = (int)((double)SAMPLE_RATE * offset);

        int samples;
        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            samples = targetSamplePos - bufferWritePos;
        }

        // render in small chunks instead of an entire event
        while (samples > 0 && !stopFlag)
        {
            int spare, writeStart;
            {
                std::lock_guard<std::mutex> lock(bufferMutex);
                spare = (bufferReadPos + bufferSize / 2) - bufferWritePos;
                writeStart = bufferWritePos;
            }

            if (spare <= 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            int chunk = std::min({ spare, samples, kMaxRenderChunkSamples });

            BassWriteWrapped(writeStart, chunk);

            {
                std::lock_guard<std::mutex> lock(bufferMutex);
                bufferWritePos += chunk;
            }

            samples -= chunk;
        }

        if (stopFlag) break;

        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            BYTE status = e.message & 0xFF;
            BYTE data1 = (e.message >> 8) & 0xFF;
            BYTE data2 = (e.message >> 16) & 0xFF;

            BYTE ev[3] = { status, data1, data2 };
            bassMidi->SendEventRaw(ev, 3);
            eventCursor++;
        }

        if (isNoteEvent && ((e.message >> 8) & 0xFF) < GetSkippingVelocity()) continue;
    }

    while (!stopFlag)
    {
        int spare, writeStart;
        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            spare = (bufferReadPos + bufferSize / 2) - bufferWritePos;
            writeStart = bufferWritePos;
        }

        if (spare > 0)
        {
            int chunk = std::min(spare, kMaxRenderChunkSamples);
            BassWriteWrapped(writeStart, chunk);

            std::lock_guard<std::mutex> lock(bufferMutex);
            bufferWritePos += chunk;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    bassMidi.reset();
}

void PrerenderedEngine::TimerLoop()
{
    bool lastPaused = false;
    float lastTimerTime = -1.0f;
    while (!timerCheckStop)
    {
        std::shared_ptr<MIDITimer> currentTimer;
        {
            std::lock_guard<std::mutex> lock(timerMutex);
            currentTimer = timer;
        }

        if (currentTimer)
        {
            const float currTime = currentTimer->Elapsed();
            const bool paused = currentTimer->IsPaused();

            if (currentTimer->HasNavigatedRecently())
            {
                const bool seekBack = currTime < lastTimerTime;
                StartPrerender(seekBack, currTime);
                lastTimerTime = currTime;
            }

            if (lastPaused && !paused)
            {
                const bool seekBack = currTime < currentTimer->GetPauseTime();
                StartPrerender(seekBack, currTime);
                lastTimerTime = currTime;
            }

            lastPaused = paused;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void PrerenderedEngine::ResetEvents()
{
    for (int ch = 0; ch < 16; ch++)
    {
        BYTE ev1[3] = { (BYTE)(0xB0u | ch), 121, 0 }; bassMidi->SendEventRaw(ev1, 3);
        BYTE ev2[3] = { (BYTE)(0xB0u | ch), 123, 0 }; bassMidi->SendEventRaw(ev2, 3);
    }
}

void PrerenderedEngine::KillLastGenerator()
{
    memset(audioBuffer, 0, bufferSize * sizeof(float));
    stopFlag = true;

    if (generatorThread.joinable())
    {
        generatorThread.join();
    }

    if (renderThread.joinable())
    {
        renderThread.join();
    }

    if (playbackStream)
    {
        BASS_ChannelSetPosition(playbackStream, 0, BASS_POS_BYTE);
    }
}

void PrerenderedEngine::StartPrerender(bool force, double time)
{
    if (!force)
    {
        const double playerTime = GetPlayerTime();
        if (time + 0.1 > playerTime + GetBufferSeconds() || time + 0.01 < playerTime)
        {
            force = true;
        }
    }

    if (force)
    {
        if (!eventsReady && !firstLoad) return;

        if (renderThread.joinable())
        {
            stopFlag = true;
            renderThread.join();
        }

        stopFlag = false;
        {
            std::lock_guard<std::mutex> lock(timerMutex);
            startTime = time;
        }
        renderThread = std::thread(&PrerenderedEngine::RenderLoop, this, startTime);
    }
    else
    {
        SyncPlayer(time);
    }
}

DWORD CALLBACK PrerenderedEngine::PlaybackStreamProc(HSTREAM handle, void* buffer, DWORD length, void* user) {
    auto* engine = static_cast<PrerenderedEngine*>(user);
    if (engine->awaitingReset)
    {
        memset(buffer, 0, length);
        return length;
    }

    std::shared_ptr<MIDITimer> currentTimer;
    {
        std::lock_guard<std::mutex> lock(engine->timerMutex);
        currentTimer = engine->timer;
    }

    std::lock_guard<std::mutex> lock(engine->bufferMutex);

    if (engine->muted || !engine->isPlaying || engine->awaitingReset || (currentTimer && currentTimer->IsPaused())) {
        memset(buffer, 0, length);
        return length;
    }

    float* fltBuffer = static_cast<float*>(buffer);
    int count = length / sizeof(float);

    int readPos = engine->bufferReadPos % (engine->bufferSize / 2);
    int writePos = engine->bufferWritePos % (engine->bufferSize / 2);
    if (engine->bufferReadPos + count / 2 > engine->bufferWritePos)
    {
        int copyCount = engine->bufferWritePos - engine->bufferReadPos;

        // safety clamp just in case it snapped past
        if (copyCount < 0) copyCount = 0;
        if (copyCount > count / 2) copyCount = count / 2;

        if (copyCount > 0)
        {
            engine->WrappedCopy(engine->audioBuffer, readPos * 2, engine->bufferSize, fltBuffer, 0, copyCount * 2);
        }

        for (int i = copyCount * 2; i < count; i++)
        {
            fltBuffer[i] = 0;
        }
    }
    else
    {
        engine->WrappedCopy(engine->audioBuffer, readPos * 2, engine->bufferSize, fltBuffer, 0, count);
    }

    engine->bufferReadPos += count / 2;

    double attack = (double)SAMPLE_RATE * engine->attackRate;
    double falloff = (double)SAMPLE_RATE * engine->releaseRate;

    for (int i = 0; i < count; i += 2)
    {
        double l = (double)std::abs(fltBuffer[i]);
        double r = (double)std::abs(fltBuffer[i + 1]);

        if (engine->loudnessL > l) engine->loudnessL = (engine->loudnessL * falloff + l) / (falloff + 1.0);
        else engine->loudnessL = (engine->loudnessL * attack + l) / (attack + 1.0);

        if (engine->loudnessR > r) engine->loudnessR = (engine->loudnessR * falloff + r) / (falloff + 1.0);
        else engine->loudnessR = (engine->loudnessR * attack + r) / (attack + 1.0);

        if (engine->loudnessL < engine->minThresh) engine->loudnessL = engine->minThresh;
        if (engine->loudnessR < engine->minThresh) engine->loudnessR = engine->minThresh;

        l = fltBuffer[i] / (engine->loudnessL * engine->strength + 2.0 * (1.0 - engine->strength)) / 2.0;
        r = fltBuffer[i + 1] / (engine->loudnessR * engine->strength + 2.0 * (1.0 - engine->strength)) / 2.0;

        if (i != 0)
        {
            double dl = std::abs((double)fltBuffer[i] - l);
            double dr = std::abs((double)fltBuffer[i + 1] - r);

            if (engine->velocityL > dl) engine->velocityL = (engine->velocityL * falloff + dl) / (falloff + 1.0);
            else engine->velocityL = (engine->velocityL * attack + dl) / (attack + 1.0);

            if (engine->velocityR > dr) engine->velocityR = (engine->velocityR * falloff + dr) / (falloff + 1.0);
            else engine->velocityR = (engine->velocityR * attack + dr) / (attack + 1.0);
        }

        if (engine->reduceHighPitch)
        {
            if (engine->velocityL > engine->velocityThresh) l = l / engine->velocityL * engine->velocityThresh;
            if (engine->velocityR > engine->velocityThresh) r = r / engine->velocityR * engine->velocityThresh;
        }

        fltBuffer[i] = (float)(l * engine->preVolume);
        fltBuffer[i + 1] = (float)(r * engine->preVolume);
    }

    return length;
}

void PrerenderedEngine::EnsureDefaultSoundfont()
{
    if (soundfonts.GetSoundfontCount() > 0) return;

    WCHAR binPath[512]{ };
    GetModuleFileNameW(NULL, binPath, 512);
    std::wstring::size_type pos = std::wstring(binPath).find_last_of(L"\\/");
    std::wstring defaultPath = std::wstring(binPath).substr(0, pos) +
        L"\\assets\\soundfonts\\GeneralUser-GS.sf2";

    if (!Utils::FileExists(defaultPath.c_str()))
    {
        std::filesystem::create_directories(std::filesystem::path(defaultPath).parent_path());

        HRSRC rc = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_SOUNDFONT), MAKEINTRESOURCEW(10));
        if (!rc) throw std::runtime_error("Failed to load embedded Soundfont");

        HGLOBAL rcData = LoadResource(NULL, rc);
        int rSize = SizeofResource(NULL, rc);
        unsigned char* data = (unsigned char*)LockResource(rcData);
        if (!data) throw std::runtime_error("That's funny... the embedded soundfont data failed to load!");

        std::ofstream file;
        file.open(defaultPath, std::ios::binary);
        file.write((char*)data, rSize);
        file.close();
    }

    soundfonts.AddSoundfont(defaultPath);
}

void PrerenderedEngine::LoadSoundfonts(std::vector<std::wstring> paths)
{
    BASSMIDI::LoadSoundfonts(paths);
}

void PrerenderedEngine::WrappedCopy(float* src, int pos, int srcCount, float* dst, int pos2, int count)
{
    if (pos + count > srcCount)
    {
        memcpy(dst + pos2, src + pos, (srcCount - pos) * sizeof(float));
        count -= (srcCount - pos);
        pos = 0;
    }
    memcpy(dst + pos2, src + pos, count * sizeof(float));
}

void PrerenderedEngine::ResizeBuffer(double newBufferLengthSecs)
{
    if (newBufferLengthSecs < BUFFER_SECS_MIN) newBufferLengthSecs = BUFFER_SECS_MIN;
    if (newBufferLengthSecs > BUFFER_SECS_MAX) newBufferLengthSecs = BUFFER_SECS_MAX;

    awaitingReset.store(true);

    bool wasPlaying = isPlaying;
    if (renderThread.joinable())
    {
        stopFlag = true;
        renderThread.join();
    }

    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        free(audioBuffer);

        bufferSeconds = newBufferLengthSecs;
        bufferSize = SAMPLE_RATE * 2 * newBufferLengthSecs;
        audioBuffer = (float*)malloc(bufferSize * sizeof(float));
        memset(audioBuffer, 0, bufferSize * sizeof(float));

        bufferWritePos = 0;
        bufferReadPos = 0;
    }

    awaitingReset.store(false);

    if (wasPlaying && seq && timer)
    {
        StartPrerender(true, GetPlayerTime());
    }
}

void PrerenderedEngine::RenderSettings()
{
    float volume = preVolume.load();
    if (ImGuiKnobs::Knob("Volume", &volume, 0.0f, 1.0f))
    {
        preVolume.store(volume);
    }

    if (ImGui::BeginTabBar("##prerendererSettings"))
    {
        if (ImGui::BeginTabItem("General"))
        {
            RenderGeneralTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Soundfonts"))
        {
            RenderSoundfontsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Limiter"))
        {
            RenderLimiterTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (std::exchange(dirty, false))
    {
        bool wasPlaying = IsPlaying();
        if (wasPlaying) Stop();

        if (std::exchange(sfDirty, false))
        {
            EnsureDefaultSoundfont(); // list emptied entirely -> fall back to the bundled one
            appliedSoundfontPaths = soundfonts.GetActiveSoundfontPaths();
            LoadSoundfonts(appliedSoundfontPaths);
        }

        bassMidi = std::make_unique<BASSMIDI>(maxVoices, noFx);

        if (wasPlaying) Start(seq, timer);
    }
}

YAML::Node PrerenderedEngine::GetSettings()
{
    YAML::Node node;
    node["maxVoices"] = maxVoices;
    node["noFx"] = noFx;
    node["bufferSeconds"] = bufferSeconds;

    YAML::Node sfNode(YAML::NodeType::Sequence);
    soundfonts.ForEachSoundfont([&](const SoundfontEntry& sf)
        {
            YAML::Node entry;
            entry["path"] = ConvertFromWideString(sf.path);
            entry["enabled"] = sf.enabled;
            sfNode.push_back(entry);
        });
    node["soundfonts"] = sfNode;

    node["attackRate"] = attackRate;
    node["releaseRate"] = releaseRate;
    node["reduceHighPitch"] = reduceHighPitch;
    node["strength"] = strength;
    node["minThresh"] = minThresh;
    node["velocityThresh"] = velocityThresh;
    
    node["simulatedFps"] = fps;

    return node;
}

void PrerenderedEngine::LoadSettings(const YAML::Node& node)
{
    if (!node) return;

    if (node["maxVoices"]) maxVoices = node["maxVoices"].as<int>();
    if (node["noFx"]) noFx = node["noFx"].as<bool>();
    if (node["bufferSeconds"]) bufferSeconds = node["bufferSeconds"].as<double>();

    if (node["soundfonts"] && node["soundfonts"].IsSequence())
    {
        std::vector<SoundfontEntry> entries;
        for (const auto& sf : node["soundfonts"])
        {
            SoundfontEntry entry;
            if (sf.IsScalar())                       // legacy: plain path string
            {
                entry.path = ConvertToWideString(sf.as<std::string>());
            }
            else if (sf.IsMap() && sf["path"])
            {
                entry.path = ConvertToWideString(sf["path"].as<std::string>());
                if (sf["enabled"]) entry.enabled = sf["enabled"].as<bool>();
            }
            else continue;

            entries.push_back(std::move(entry));
        }
        soundfonts.LoadFromEntries(std::move(entries));
    }

    if (node["attackRate"]) attackRate = node["attackRate"].as<double>();
    if (node["releaseRate"]) releaseRate = node["releaseRate"].as<double>();
    if (node["reduceHighPitch"]) reduceHighPitch = node["reduceHighPitch"].as<bool>();
    if (node["strength"]) strength = node["strength"].as<double>();
    if (node["minThresh"]) minThresh = node["minThresh"].as<double>();
    if (node["velocityThresh"]) velocityThresh = node["velocityThresh"].as<double>();

    if (node["simulatedFps"]) fps = node["simulatedFps"].as<double>();

    ResizeBuffer(bufferSeconds);
}

#pragma region Tabs

void PrerenderedEngine::RenderGeneralTab()
{
    std::string memoryUsage = Utils::FormatFilesize(this->bufferSize * sizeof(float), 2);

    BEGIN_SECTION("##engineSettings")
    {
        SETUP_SECTION;

        SECTION_ENTRY(SECTION_LABEL("Voices"),
            {
                if (ImGui::InputInt("##maxVoices", &maxVoices))
                {
                    if (maxVoices > 100000) maxVoices = 100000;
                    if (maxVoices < 32) maxVoices = 32;
                    MarkEngineDirty();
                }
            });

        SECTION_ENTRY(
            TABLE_LABEL_TOOLTIP(
                "Disable effects",
                "Disables various effects such as reverb and chorus. This may speed up rendering."
            ),
            {
                if (ImGui::Checkbox("##noFx", &noFx)) MarkEngineDirty();

            });

        SECTION_ENTRY(SECTION_LABEL("Buffer length (s)"),
            {
                float bufferLength = this->bufferSeconds;
                if (ImGui::SliderFloat("##bufferLength", &bufferLength, BUFFER_SECS_MIN, BUFFER_SECS_MAX))
                {
                    ResizeBuffer(bufferLength);
                    MarkEngineDirty();
                }
                ImGui::Text("RAM Usage: %s", memoryUsage);
            });

        SECTION_ENTRY(
            TABLE_LABEL_TOOLTIP(
                "Simulated RT FPS",
                "Set to 0 for sample-accurate timing. Values above 1 will snap events to the nearest \"frame\" at an interval of whatever was set."
            ),
            {
                float fps = (float)this->fps;
                if (ImGui::InputFloat("##rtFps", &fps))
                {
                    if (fps < 1.0) fps = 0.0f;
                    if (fps > 1000.0) fps = 1000.0f;
                    this->fps = (double)fps;
                    MarkEngineDirty();
                }
            });

        END_SECTION;
    }
}

void PrerenderedEngine::RenderSoundfontsTab()
{
    const bool dirty = soundfonts.GetActiveSoundfontPaths() != appliedSoundfontPaths;
    const int sfCount = (int)soundfonts.GetSoundfontCount();

    ImGui::Text("Soundfont list");

    ImGui::PushID("##soundfontList");
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
    ImGui::BeginChild("##container", ImVec2(-FLT_MIN, 140), ImGuiChildFlags_Borders);
    {
        const bool canMoveUp = selectedSoundfontIndex > 0;
        const bool canMoveDown = selectedSoundfontIndex >= 0 && selectedSoundfontIndex < sfCount - 1;

        int moveDelta = 0;
        bool removeSelected = false;

        if (ImGui::BeginTable("##table", 2, ImGuiTableFlags_ScrollY, ImVec2(-FLT_MIN, -FLT_MIN)))
        {
            ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);

            for (int i = 0; i < sfCount; i++)
            {
                SoundfontEntry* entry = soundfonts.GetSoundfont(i);
                if (!entry) continue;

                ImGui::PushID(i);
                ImGui::TableNextRow();

                // shade every other row
                if ((i & 0b1) != 0)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_Text, 0.075f));

                ImGui::TableSetColumnIndex(0);
                ImGui::Checkbox("##enabled", &entry->enabled);

                ImGui::TableSetColumnIndex(1);
                std::string fileName = ConvertFromWideString(std::filesystem::path(entry->path).filename().wstring());
                if (fileName.empty()) fileName = ConvertFromWideString(entry->path);

                // right edge of this cell, in cursor coordinates (grab it before the Selectable moves the cursor)
                const float cellRightScreen = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;

                if (!entry->enabled) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

                // AllowOverlap lets the buttons below sit on top of the row and still receive clicks
                if (ImGui::Selectable(fileName.c_str(), selectedSoundfontIndex == i, ImGuiSelectableFlags_AllowOverlap))
                    selectedSoundfontIndex = i;

                if (!entry->enabled) ImGui::PopStyleColor();

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", ConvertFromWideString(entry->path).c_str());

                // up/down/delete buttons
                if (selectedSoundfontIndex == i)
                {
                    const ImGuiStyle& style = ImGui::GetStyle();
                    const char* labels[] = { "^", "v", "-" };
                    float totalWidth = style.ItemSpacing.x * 2;
                    for (const char* l : labels)
                        totalWidth += ImGui::CalcTextSize(l).x + style.FramePadding.x * 2;

                    // vertically center the small buttons against the row, then place them flush right
                    const float rowTop = ImGui::GetItemRectMin().y;
                    const float rowH = ImGui::GetItemRectSize().y;
                    const float btnH = ImGui::GetFontSize(); // SmallButton uses zero vertical padding
                    ImGui::SetCursorScreenPos(ImVec2(cellRightScreen - totalWidth, rowTop + (rowH - btnH) * 0.5f));

                    ImGui::BeginDisabled(!canMoveUp);
                    if (ImGui::SmallButton("^")) moveDelta = -1;
                    ImGui::EndDisabled();

                    ImGui::SameLine();
                    ImGui::BeginDisabled(!canMoveDown);
                    if (ImGui::SmallButton("v")) moveDelta = 1;
                    ImGui::EndDisabled();

                    ImGui::SameLine();
                    if (ImGui::SmallButton("-")) removeSelected = true;
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        // now it's safe to modify the list
        if (moveDelta != 0 && soundfonts.SwapSoundfonts(selectedSoundfontIndex, selectedSoundfontIndex + moveDelta))
        {
            selectedSoundfontIndex += moveDelta;
        }
        else if (removeSelected)
        {
            soundfonts.RemoveSoundfont(selectedSoundfontIndex);
            selectedSoundfontIndex = std::min(selectedSoundfontIndex, (int)soundfonts.GetSoundfontCount() - 1);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopID();

    if (ImGui::Button("+"))
    {
        std::string sfPath;
        if (Utils::ChooseFile(sfPath, "sf2,sf3,sfz,sfark") && !sfPath.empty())
        {
            soundfonts.AddSoundfont(ConvertToWideString(sfPath));
            selectedSoundfontIndex = (int)soundfonts.GetSoundfontCount() - 1;
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!dirty);
    if (ImGui::Button("Apply"))
    {
        MarkEngineDirty();
        MarkSoundfontsDirty();
    }
    ImGui::EndDisabled();

    if (dirty)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Unapplied changes");
    }
    if (soundfonts.GetActiveSoundfontPaths().empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "No soundfonts are enabled.");
    }
}

void PrerenderedEngine::RenderLimiterTab()
{
    BEGIN_SECTION("##limiter")
    {
        SETUP_SECTION;

        SECTION_ENTRY(SECTION_LABEL("Attack rate (s)"),
            {
                float attackRate = this->attackRate;
                ImGui::SliderFloat("##limAttack", &attackRate, 0.001, 0.01);
                if (attackRate < 0.001) attackRate = 0.001;
                if (attackRate > 0.01) attackRate = 0.01;
                this->attackRate = attackRate;
            });

        SECTION_ENTRY(SECTION_LABEL("Release rate (s)"),
            {
                float releaseRate = this->releaseRate;
                ImGui::SliderFloat("##limRelease", &releaseRate, 0.1, 2.0);
                if (releaseRate < 0.1) releaseRate = 0.1;
                if (releaseRate > 2.0) releaseRate = 2.0;
                this->releaseRate = releaseRate;
            });

        END_SECTION;
    }
}

#pragma endregion

#endif