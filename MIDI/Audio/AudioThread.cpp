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

    bool operator<(const PrecalculatedEvent& other) const noexcept
    {
        if (tick != other.tick) return tick < other.tick;
        return (message & 0xF0) < (other.message & 0xF0);
    }
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

        for (const auto& ev : seq->mergedEvents)
        {
            events.push_back({ ev.tick, ev.message });
        }

        std::sort(events.begin(), events.end());

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
                for (int ch = 0; ch < 16; ch++)
                {
                    midiOut->SendEvent(0xB0 | ch | (64 << 8));
                    // midiOut->SendEvent(
                    //     0xE0 | ch |
                    //     (0x00 << 8) |
                    //     (0x40 << 16)
                    // );
                }

                syncToTick(currPos);
            }

            if (navigated || timer->PauseRequestedRecently())
            {
                for (int ch = 0; ch < 16; ch++)
                {
                    midiOut->SendEvent(0xB0 | ch | (123 << 8));
                }

                std::this_thread::sleep_for(std::chrono::microseconds(500));
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

        for (int ch = 0; ch < 16; ch++)
        {
            midiOut->SendEvent(0xB0 | ch | (64 << 8));
            midiOut->SendEvent(0xB0 | ch | (123 << 8));
        }
    });

    this->audioThread = std::move(audioThread);
    threadWorking = true;
}