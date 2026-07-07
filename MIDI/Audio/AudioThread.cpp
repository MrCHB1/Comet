// idk how but this is blazingly fast

#include "AudioThread.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <chrono>
#include "MIDI/TempoMap.h"
#include "MIDI/MIDIDefs.h"

struct PrecalculatedEvent
{
    long tick;
    uint32_t message;
};

void AudioThread::Start(std::shared_ptr<MIDISequence> seq, std::shared_ptr<MIDITimer> timer)
{
    if (!seq || !timer) return;
    stopFlag = false;

    std::thread audioThread([this, seq, timer]() {
        auto* tempoMap = seq->GetTempoMap();
        const auto resolution = seq->resolution;

        std::vector<PrecalculatedEvent> events;
        events.reserve(seq->notes * 2 + seq->mergedEvents.size());

        for (const auto& ev : seq->mergedEvents)
        {
            events.push_back({ ev.tick, ev.message });
        }

        for (int i = 0; i < MIDI_KEYS; ++i)
        {
            const auto& notes = seq->mergedNotes[i];
            for (const auto& note : notes)
            {
                events.push_back({
                    note.tick,
                    0x90u | note.channel | ((uint32_t)note.note << 8) | ((uint32_t)note.vel << 16)
                    });

                events.push_back({
                    note.tick + (long)note.gate,
                    0x80u | note.channel | ((uint32_t)note.note << 8)
                    });
            }
        }

        std::stable_sort(events.begin(), events.end(), [](const PrecalculatedEvent& a, const PrecalculatedEvent& b) { return a.tick < b.tick; });

        size_t cursor = 0;
        uint32_t msg = 0;

        auto syncToTick = [&](long tick)
            {
                cursor = (size_t)(std::lower_bound(
                    events.begin(),
                    events.end(),
                    tick,
                    [](const PrecalculatedEvent& ev, long pos)
                    {
                        return ev.tick < pos;
                    }) - events.begin());

                for (const auto& ev : events)
                {
                    if (ev.tick > tick) break;
                    if ((ev.message & 0xF0) == 0x80 || (ev.message & 0xF0) == 0x90) continue;
                    midiOut->SendEvent(ev.message);
                }
            };

        while (!stopFlag)
        {
            if (audioMuted)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(500));
                continue;
            }

            const bool navigated = timer->HasNavigatedRecently();
            const long currentTick = (long)tempoMap->SecsToTicksFromMap(resolution, timer->Elapsed());
            long currPos = seq->timeBased ? (long)(timer->Elapsed() * TIME_BASED_MULTIPLIER) : (double)currentTick;

            if (navigated)
            {
                ResetEvents();

                syncToTick(currPos);
            }

            if (navigated || timer->PauseRequestedRecently())
            {
                for (int ch = 0; ch < 16; ch++)
                {
                    midiOut->SendEvent(0xB0 | ch | (123 << 8));
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (!timer->IsPaused())
            {
                while (cursor < events.size() && events[cursor].tick <= currPos)
                {
                    if (stopFlag || audioMuted) break;

                    const auto& ev = events[cursor++];
                    midiOut->SendEvent(ev.message);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }

        ResetEvents();

        threadWorking = false;
    });

    this->audioThread = std::move(audioThread);
    threadWorking = true;
}

void AudioThread::ResetEvents()
{
    // hardcoded way to reset CC
    for (int ch = 0; ch < 16; ch++)
    {
        midiOut->SendEvent(0xB0u | ch);
        midiOut->SendEvent(0xB0u | ch | (1u << 8u));
        midiOut->SendEvent(0xB0u | ch | (2u << 8u));
        midiOut->SendEvent(0xB0u | ch | (4u << 8u));
        midiOut->SendEvent(0xB0u | ch | (5u << 8u));
        midiOut->SendEvent(0xB0u | ch | (7u << 8u) | (100u << 16u)); // volume
        midiOut->SendEvent(0xB0u | ch | (8u << 8u) | (64u << 16u));
        midiOut->SendEvent(0xB0u | ch | (10u << 8u) | (64u << 16u)); // pan
        midiOut->SendEvent(0xB0u | ch | (11u << 8u) | (127u << 16u)); // expression

        // sustain, portamento, sostenuto, soft pedal, etc..
        for (unsigned int cc = 64; cc <= 69; cc++)
        {
            midiOut->SendEvent(0xB0u | ch | (cc << 8u));
        }

        for (unsigned int cc = 70; cc <= 79; cc++)
        {
            midiOut->SendEvent(0xB0u | ch | (cc << 8u) | (64u << 16u)); // sound controllers
        }

        midiOut->SendEvent(0xB0u | ch | (91u << 8u) | (40u << 16u)); // reverb

        for (unsigned int cc = 92; cc <= 95; cc++)
        {
            midiOut->SendEvent(0xB0u | ch | (cc << 8u));
        }

        midiOut->SendEvent(0xB0u | ch | (101u << 8u) | (127u << 16u));
        midiOut->SendEvent(0xB0u | ch | (100u << 8u) | (127u << 16u));
        midiOut->SendEvent(0xB0u | ch | (99u << 8u) | (127u << 16u));
        midiOut->SendEvent(0xB0u | ch | (98u << 8u) | (127u << 16u));

        // just in case, send the "reset all controllers" event too
        midiOut->SendEvent(0xB0 | ch | (121 << 8));
        midiOut->SendEvent(0xB0 | ch | (123 << 8));
    }
}