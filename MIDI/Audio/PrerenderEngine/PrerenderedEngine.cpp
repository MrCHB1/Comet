#if defined(WIN32)

#include "PrerenderedEngine.h"
#include <imgui.h>
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

    std::vector<std::wstring> widePaths;
    for (const auto& p : soundfontPaths)
    {
        widePaths.push_back(ConvertToWideString(p));
    }
    LoadSoundfonts(widePaths);

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
            uint8_t nNote = notes.note[j];
            uint8_t nChannel = notes.channel[j];
            uint8_t nVel = notes.vel[j];

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

        // render in small, bounded chunks and NEVER hold bufferMutex while
        // synthesizing. bassMidi->Read() can take a while in dense/heavy
        // passages (lots of active voices); if we held the lock across that
        // call, the realtime PlaybackStreamProc callback would have to wait
        // on us too, which is exactly what produces the audible delay when
        // seeking into a heavy section. capping the chunk size also means we
        // re-check stopFlag frequently, so a seek (which sets stopFlag and
        // joins this thread) doesn't have to wait out a big pending render.
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
    if (!eventsReady) return;

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

        double scaleL = engine->loudnessL * engine->strength + 2.0 * (1.0 - engine->strength);
        double scaleR = engine->loudnessR * engine->strength + 2.0 * (1.0 - engine->strength);

        if (scaleL < 1.0) scaleL = 1.0;
        if (scaleR < 1.0) scaleR = 1.0;

        l = fltBuffer[i] / scaleL;
        r = fltBuffer[i + 1] / scaleR;

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

void PrerenderedEngine::LoadSoundfonts(std::vector<std::wstring> paths)
{
    std::wstring soundfontPath;
    if (paths.empty())
    {
        WCHAR binPath[512]{ };
        GetModuleFileNameW(NULL, binPath, 512);
        std::wstring::size_type pos = std::wstring(binPath).find_last_of(L"\\/");
        std::wstring defaultPath = std::wstring(binPath).substr(0, pos) +
            L"\\assets\\soundfonts\\GeneralUser-GS.sf2";

        if (!Utils::FileExists(defaultPath.c_str()))
        {
            std::filesystem::create_directories(std::filesystem::path(defaultPath).parent_path());

            HRSRC rc = FindResourceW(
                NULL,
                MAKEINTRESOURCEW(IDR_SOUNDFONT),
                MAKEINTRESOURCEW(10)
            );
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
        paths = { defaultPath };
        soundfontPaths = { ConvertFromWideString(defaultPath) };
    }

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
    bool shouldApplyChanges = false;
    bool shouldChangeSoundfont = false;
    if (ImGui::BeginTabBar("##prerendererSettings"))
    {
        if (ImGui::BeginTabItem("General"))
        {
            SECTION_HEADER("Engine Settings");
            BEGIN_SECTION("##engineSettings")
            {
                SETUP_SECTION;

                SECTION_ENTRY(SECTION_LABEL("Voices"),
                    {
                        if (ImGui::InputInt("##maxVoices", &maxVoices))
                        {
                            if (maxVoices > 100000) maxVoices = 100000;
                            if (maxVoices < 32) maxVoices = 32;
                            shouldApplyChanges = true;
                        }
                    });

                SECTION_ENTRY(
                    TABLE_LABEL_TOOLTIP(
                        "Disable effects",
                        "Disables various effects such as reverb and chorus. This may speed up rendering."
                    ),
                    {
                        if (ImGui::Checkbox("##noFx", &noFx)) shouldApplyChanges = true;

                    });

                SECTION_ENTRY(SECTION_LABEL("Buffer length (s)"),
                    {
                        float bufferLength = this->bufferSeconds;
                        if (ImGui::SliderFloat("##bufferLength", &bufferLength, BUFFER_SECS_MIN, BUFFER_SECS_MAX))
                        {
                            ResizeBuffer(bufferLength);
                            shouldApplyChanges = true;
                        }
                        std::string memoryUsage = Utils::FormatFilesize(this->bufferSize * sizeof(float), 2);
                        ImGui::Text("RAM Usage: %s", memoryUsage);
                    });

                END_SECTION;
            }

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Soundfonts"))
        {
            ImGui::Text("Soundfont list");
            if (ImGui::BeginListBox("##soundfontList", ImVec2(-FLT_MIN, 140)))
            {
                for (int i = 0; i < soundfontPaths.size(); i++)
                {
                    bool isSelected = (selectedSoundfontIndex == i);
                    std::filesystem::path p(soundfontPaths[i]);
                    std::string fileName = p.filename().string();
                    if (fileName.empty()) fileName = soundfontPaths[i];

                    std::string label = std::to_string(i + 1) + ": " + fileName + "##" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), isSelected))
                    {
                        selectedSoundfontIndex = i;
                    }

                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndListBox();
            }

            bool canMoveUp = selectedSoundfontIndex > 0;
            bool canMoveDown = selectedSoundfontIndex >= 0 && selectedSoundfontIndex < (int)soundfontPaths.size() - 1;

            ImGui::BeginDisabled(!canMoveUp);
            if (ImGui::SmallButton("^"))
            {
                std::swap(soundfontPaths[selectedSoundfontIndex], soundfontPaths[selectedSoundfontIndex - 1]);
                selectedSoundfontIndex--;
                shouldApplyChanges = true;
                shouldChangeSoundfont = true;
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            ImGui::BeginDisabled(!canMoveDown);
            if (ImGui::SmallButton("v"))
            {
                std::swap(soundfontPaths[selectedSoundfontIndex], soundfontPaths[selectedSoundfontIndex + 1]);
                selectedSoundfontIndex++;
                shouldApplyChanges = true;
                shouldChangeSoundfont = true;
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            if (ImGui::SmallButton("+"))
            {
                std::string sfPath = "";
                if (Utils::ChooseFile(sfPath, "sf2,sf3,sfz,sfark"))
                {
                    if (!sfPath.empty())
                    {
                        soundfontPaths.push_back(sfPath);
                        shouldApplyChanges = true;
                        shouldChangeSoundfont = true;
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("-"))
            {
                if (!soundfontPaths.empty())
                {
                    if (selectedSoundfontIndex >= 0 && selectedSoundfontIndex < (int)soundfontPaths.size())
                    {
                        soundfontPaths.erase(soundfontPaths.begin() + selectedSoundfontIndex);
                        if (selectedSoundfontIndex >= (int)soundfontPaths.size())
                        {
                            selectedSoundfontIndex = (int)soundfontPaths.size() - 1;
                        }
                    }
                    else
                    {
                        soundfontPaths.pop_back();
                    }
                    shouldApplyChanges = true;
                    shouldChangeSoundfont = true;
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Limiter"))
        {
            BEGIN_SECTION("##limiter")
            {
                SETUP_SECTION;
                
                SECTION_ENTRY(SECTION_LABEL("Attack rate"),
                    {
                        float attackRate = this->attackRate;
                        ImGui::SliderFloat("##limAttack", &attackRate, 0.1, 1.5);
                        if (attackRate < 0.1) attackRate = 0.1;
                        if (attackRate > 1.5) attackRate = 1.5;
                        this->attackRate = attackRate;
                    });

                SECTION_ENTRY(SECTION_LABEL("Release rate"),
                    {
                        float releaseRate = this->releaseRate;
                        ImGui::SliderFloat("##limRelease", &releaseRate, 0.005, 0.1);
                        if (releaseRate < 0.005) releaseRate = 0.005;
                        if (releaseRate > 0.1) releaseRate = 0.1;
                        this->releaseRate = releaseRate;
                    });

                END_SECTION;
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (shouldApplyChanges)
    {
        bool wasPlaying = IsPlaying();
        if (wasPlaying) Stop();

        if (shouldChangeSoundfont)
        {
            std::vector<std::wstring> widePaths;
            for (const auto& p : soundfontPaths)
            {
                widePaths.push_back(ConvertToWideString(p));
            }
            LoadSoundfonts(widePaths);
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
    for (const auto& path : soundfontPaths)
    {
        sfNode.push_back(path);
    }
    node["soundfonts"] = sfNode;

    node["attackRate"] = attackRate;
    node["releaseRate"] = releaseRate;
    node["reduceHighPitch"] = reduceHighPitch;
    node["strength"] = strength;
    node["minThresh"] = minThresh;
    node["velocityThresh"] = velocityThresh;

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
        soundfontPaths.clear();
        for (const auto& sf : node["soundfonts"])
        {
            soundfontPaths.push_back(sf.as<std::string>());
        }
    }

    if (node["attackRate"]) attackRate = node["attackRate"].as<double>();
    if (node["releaseRate"]) releaseRate = node["releaseRate"].as<double>();
    if (node["reduceHighPitch"]) reduceHighPitch = node["reduceHighPitch"].as<bool>();
    if (node["strength"]) strength = node["strength"].as<double>();
    if (node["minThresh"]) minThresh = node["minThresh"].as<double>();
    if (node["velocityThresh"]) velocityThresh = node["velocityThresh"].as<double>();

    ResizeBuffer(bufferSeconds);
}

#endif