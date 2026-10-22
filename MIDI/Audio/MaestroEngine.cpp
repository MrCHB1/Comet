#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "MIDI/MIDISequence.h"
#include "MIDI/Timer/MIDITimer.h"
#include "MIDI/TempoMap.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <vector>
#include <string>
#include "imgui.h"

#include "MIDI/Audio/MaestroEngine.h"

#if defined(_WIN32)
#include <mmsystem.h>
#include <avrt.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "avrt.lib")
#endif

namespace
{
    using Clock = std::chrono::steady_clock;

    constexpr int64_t NS_LIMIT = std::numeric_limits<int64_t>::max() / 4;
    constexpr int64_t NS_NONE = std::numeric_limits<int64_t>::max();   // "no more events"
    constexpr int64_t NS_MIN = std::numeric_limits<int64_t>::min();

    constexpr int64_t JUMP_NS = 15'000'000;        // timer vs wall clock disagreement => seek
    constexpr int64_t LATENCY_NS = 10'000'000;     // constant audio offset so the first events aren't late
    constexpr int64_t GUARD_NS = 5'000'000;        // margin under max_schedule
    constexpr int64_t POLL_NS = 5'000'000;         // player poll interval (pause/seek detection)
    constexpr int64_t LOOKAHEAD_NS = 250'000'000;  // how far ahead of "now" events are queued in Maestro
    constexpr int64_t REFILLS_PER_LOOKAHEAD = 16;   // player tops up this many times per lookahead
    constexpr int64_t GAP_NS = 1'000;              // 1 us: min note length / gap before the same key retriggers

    int64_t SecondsToNs(double seconds)
    {
        if (std::isnan(seconds))
            return 0;

        const long double ns = static_cast<long double>(seconds) * 1'000'000'000.0L;

        if (ns > NS_LIMIT) return NS_LIMIT;
        if (ns < -NS_LIMIT) return -NS_LIMIT;

        return static_cast<int64_t>(std::llroundl(ns));
    }

    uint32_t MakeMidiMessage(uint8_t status, uint8_t channel, uint8_t b1, uint8_t b2)
    {
        return
            (static_cast<uint32_t>(status | (channel & 0x0F))) |
            (static_cast<uint32_t>(b1) << 8) |
            (static_cast<uint32_t>(b2) << 16);
    }

    void MAESTRO_CALL OnMaestroLog(void*, uint32_t level, const char* message)
    {
        static const char* const names[] = { "error", "warn" };
        std::cout << std::format("[maestro {}] {}", level < 2 ? names[level] : "?", message) << std::endl;
    }
}

void MaestroEngine::Initialize()
{
    if (hasMaestro) return;

    Library lib = open_library();
    if (!lib)
    {
        std::cout << "Maestro (" LIBRARY_NAME ") is not installed." << std::endl;
        return;
    }

    MaestroGetInterfaceFn get_interface;
    *(void**)&get_interface = find_symbol(lib, "maestro_get_interface");
    if (!get_interface)
        return;

    core = (MaestroCoreV1*)get_interface(MAESTRO_CORE_V1);
    rt = (MaestroRealtimeV1*)get_interface(MAESTRO_REALTIME_V1);

    if (!core || !rt)
    {
        core = nullptr;
        rt = nullptr;
        std::cout << "Maestro is unavailable." << std::endl;
        return;
    }

    app = { sizeof app };
    app.app_id = "comet.audio.maestro";
    app.app_name = "Comet Maestro Audio";
    app.log = OnMaestroLog;

    MaestroResult r = rt->open(&app, NULL, &inst);
    if (r != MAESTRO_OK)
    {
        inst = nullptr;
        if (r == MAESTRO_ERR_DISABLED)
            std::cout << "Maestro is turned off for this application: " << core->last_error() << std::endl;
        else
            std::cout << "Open: " << core->last_error() << std::endl;
        return;
    }

    MaestroRealtimeInfo info{};
    info.struct_size = sizeof(info);
    if (rt->get_info(inst, &info) != MAESTRO_OK || info.max_schedule_us == 0)
    {
        std::cout << "Get info error: " << core->last_error() << std::endl;
        rt->close(inst);
        inst = nullptr;
        return;
    }

    maxScheduleTimeUs = info.max_schedule_us;
    hasMaestro = true;
    std::cout << "Maestro ready! max_schedule_us=" << maxScheduleTimeUs << std::endl;
}

void MaestroEngine::Destroy()
{
    if (!hasMaestro) return;

    Stop();

    if (inst)
    {
        rt->reset(inst);
        rt->close(inst);
    }

    inst = nullptr;
    rt = nullptr;
    core = nullptr;
    hasMaestro = false;
}

void MaestroEngine::Start(std::shared_ptr<MIDISequence> seq, std::shared_ptr<MIDITimer> timer)
{
    if (!seq || !timer || !hasMaestro || maxScheduleTimeUs == 0)
        return;

    Stop();

    if (ring.size() != RING_SIZE)
        ring.assign(RING_SIZE, SchedEvent{});

    head.store(0);
    tail.store(0);
    {
        std::lock_guard<std::mutex> lock(ctlMutex);
        ctlEpoch = 0;
        ctlSeekNs = 0;
    }
    epoch.store(0);
    genDoneEpoch.store(0);

    stopFlag.store(false);
    threadWorking.store(true);

    generatorThread = std::thread(
        [this, seq = std::move(seq)]() mutable
        {
            GeneratorLoop(std::move(seq));
        });

    playerThread = std::thread(
        [this, timer = std::move(timer)]() mutable
        {
#if defined(_WIN32)
            timeBeginPeriod(1);
            DWORD taskIndex = 0;
            HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
#endif
            PlayerLoop(std::move(timer));
#if defined(_WIN32)
            if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
            timeEndPeriod(1);
#endif
            threadWorking.store(false);
        });
}

void MaestroEngine::Stop()
{
    stopFlag.store(true);
    scheduleCV.notify_all();
    spaceCV.notify_all();

    if (playerThread.joinable()) playerThread.join();
    if (generatorThread.joinable()) generatorThread.join();

    threadWorking.store(false);

    // Events are queued up to LOOKAHEAD ahead inside Maestro. Without this
    // they keep playing after Stop and notes can hang.
    if (hasMaestro && inst)
        rt->reset(inst);
}

// ---------------------------------------------------------------------------------------
// Generator: turns the sequence into a time-ordered event stream, as far ahead as the ring
// allows, starting from wherever the player says playback begins.
// ---------------------------------------------------------------------------------------
void MaestroEngine::GeneratorLoop(std::shared_ptr<MIDISequence> seq)
{
    auto* tempoMap = seq->GetTempoMap();
    if (!tempoMap)
        return;

    const auto resolution = seq->resolution;
    const auto& chEvs = seq->mergedEvents;

    auto tickNs = [&](uint64_t tick) -> int64_t
    {
        return SecondsToNs(tempoMap->TicksToSecsFromMap(resolution, tick));
    };

    struct PendingNoteOff
    {
        int64_t timeNs;
        uint64_t order;
        uint32_t message;
    };

    struct PendingNoteOffLater
    {
        bool operator()(const PendingNoteOff& a, const PendingNoteOff& b) const
        {
            if (a.timeNs != b.timeNs)
                return a.timeNs > b.timeNs;
            return a.order > b.order;
        }
    };

    struct NoteHead
    {
        int64_t ns;
        uint32_t key;
    };

    struct NoteHeadLater
    {
        bool operator()(const NoteHead& a, const NoteHead& b) const
        {
            if (a.ns != b.ns)
                return a.ns > b.ns;
            return a.key > b.key;
        }
    };

    std::priority_queue<PendingNoteOff, std::vector<PendingNoteOff>, PendingNoteOffLater> pendingOffs;
    std::priority_queue<NoteHead, std::vector<NoteHead>, NoteHeadLater> noteHeads;
    uint64_t order = 0;

    size_t chCursor = 0;
    int64_t chNextNs = NS_NONE;
    std::array<size_t, MIDI_KEYS> noteCursor{};

    uint64_t myEpoch = 0;
    bool done = false;

    auto genWait = [&]
    {
        std::unique_lock<std::mutex> lock(spaceMutex);
        spaceCV.wait_for(lock, std::chrono::milliseconds(2), [&] {
            return stopFlag.load() || epoch.load(std::memory_order_acquire) != myEpoch;
        });
    };

    // false = aborted (stop requested or a newer epoch exists)
    auto push = [&](int64_t ns, uint32_t message) -> bool
    {
        const size_t h = head.load(std::memory_order_relaxed);

        while (h - tail.load(std::memory_order_acquire) >= RING_SIZE)
        {
            if (stopFlag.load() || epoch.load(std::memory_order_acquire) != myEpoch)
                return false;

            std::unique_lock<std::mutex> lock(spaceMutex);
            spaceCV.wait_for(lock, std::chrono::milliseconds(2), [&] {
                return stopFlag.load()
                    || h - tail.load(std::memory_order_acquire) < RING_SIZE
                    || epoch.load(std::memory_order_acquire) != myEpoch;
            });
        }

        ring[h & RING_MASK] = SchedEvent{ ns, message, static_cast<uint32_t>(myEpoch) };
        head.store(h + 1, std::memory_order_release);
        return true;
    };

    // Positions every cursor at the start time the player published and emits the
    // controller / program / pitch bend state needed to play from there.
    auto reposition = [&]() -> bool
    {
        int64_t startNs;
        {
            std::lock_guard<std::mutex> lock(ctlMutex);
            myEpoch = ctlEpoch;
            startNs = ctlSeekNs;
        }

        pendingOffs = decltype(pendingOffs)();
        noteHeads = decltype(noteHeads)();

        {
            size_t lo = 0, hi = chEvs.size();
            while (lo < hi)
            {
                const size_t mid = lo + (hi - lo) / 2;
                if (tickNs(chEvs[mid].tick) < startNs) lo = mid + 1;
                else hi = mid;
            }
            chCursor = lo;
            chNextNs = chCursor < chEvs.size() ? tickNs(chEvs[chCursor].tick) : NS_NONE;
        }

        for (size_t i = 0; i < MIDI_KEYS; ++i)
        {
            auto& notes = seq->mergedNotes[i];

            size_t lo = 0, hi = notes.Size();
            while (lo < hi)
            {
                const size_t mid = lo + (hi - lo) / 2;
                if (tickNs(notes.tick[mid]) < startNs) lo = mid + 1;
                else hi = mid;
            }

            noteCursor[i] = lo;
            if (lo < notes.Size())
                noteHeads.push({ tickNs(notes.tick[lo]), static_cast<uint32_t>(i) });
        }

        // state restore: latest CC / program / pressure / bend before startNs, 1 us apart
        // so their order survives however Maestro breaks ties
        std::map<uint32_t, uint32_t> latest;
        for (size_t i = 0; i < chCursor; ++i)
        {
            const uint32_t msg = chEvs[i].message;
            const uint32_t status = msg & 0xFF;
            const uint32_t type = status & 0xF0;

            if (type == 0xB0)
                latest[status | (((msg >> 8) & 0xFF) << 8)] = msg;
            else if (type == 0xC0 || type == 0xD0 || type == 0xE0)
                latest[status] = msg;
        }

        int64_t n = 0;
        for (int pass = 0; pass < 2; ++pass)   // CCs first (bank select), then program changes
        {
            for (const auto& [key, msg] : latest)
            {
                const bool isProgram = (msg & 0xF0) == 0xC0;
                if (isProgram != (pass == 1)) continue;
                if (!push(startNs + (n++) * GAP_NS, msg)) return false;
            }
        }

        return true;
    };

    while (!stopFlag.load())
    {
        if (epoch.load(std::memory_order_acquire) != myEpoch)
        {
            done = false;
            if (!reposition())
                continue;   // aborted: the loop re-checks stop / epoch
        }

        if (myEpoch == 0 || done)
        {
            genWait();
            continue;
        }

        // next event over all sources; ties: note-offs, then channel events, then note-ons
        enum Source { None, Off, Channel, Note } src = None;
        int64_t best = NS_NONE;

        if (!pendingOffs.empty())
        {
            best = pendingOffs.top().timeNs;
            src = Off;
        }

        if (chNextNs < best)
        {
            best = chNextNs;
            src = Channel;
        }

        if (!noteHeads.empty() && noteHeads.top().ns < best)
        {
            best = noteHeads.top().ns;
            src = Note;
        }

        switch (src)
        {
        case None:
            genDoneEpoch.store(myEpoch, std::memory_order_release);
            done = true;
            break;

        case Off:
        {
            const uint32_t msg = pendingOffs.top().message;
            pendingOffs.pop();
            push(best, msg);
            break;
        }

        case Channel:
        {
            const uint32_t msg = chEvs[chCursor].message;
            ++chCursor;
            chNextNs = chCursor < chEvs.size() ? tickNs(chEvs[chCursor].tick) : NS_NONE;
            push(best, msg);
            break;
        }

        case Note:
        {
            const uint32_t key = noteHeads.top().key;
            noteHeads.pop();

            auto& notes = seq->mergedNotes[key];
            auto& cursor = noteCursor[key];

            const auto tick = notes.tick[cursor];
            const uint8_t noteKey = notes.GetKey(cursor);
            const uint8_t channel = notes.GetChannel(cursor);
            const uint8_t velocity = notes.GetVelocity(cursor);
            const uint64_t gate = notes.gate[cursor];

            const int64_t onNs = best;
            int64_t offNs = tickNs(static_cast<uint64_t>(tick) + gate);

            ++cursor;
            if (cursor < notes.Size())
            {
                const int64_t nextOnNs = tickNs(notes.tick[cursor]);
                noteHeads.push({ nextOnNs, key });

                offNs = std::min(offNs, nextOnNs);
            }
            offNs = std::max(offNs, onNs);   // a note is at least 1 us long

            pendingOffs.push({ offNs, order++, MakeMidiMessage(0x80, channel, noteKey, 0) });
            push(onNs, MakeMidiMessage(0x90, channel, noteKey, velocity));
            break;
        }
        }
    }
}

// ---------------------------------------------------------------------------------------
// Player: wakes on an interval and hands everything due in the next lookahead window to
// Maestro in one pass.
// ---------------------------------------------------------------------------------------
void MaestroEngine::PlayerLoop(std::shared_ptr<MIDITimer> timer)
{
    const int64_t maxNs = static_cast<int64_t>(std::min<uint64_t>(maxScheduleTimeUs, NS_LIMIT / 1000)) * 1000;
    const int64_t latencyNs = std::min(LATENCY_NS, maxNs / 20);
    const int64_t guardNs = std::min(GUARD_NS, maxNs / 20);

    // never reach further than Maestro allows, but also not further than LOOKAHEAD_NS
    const int64_t aheadNs = std::min(maxNs - guardNs - latencyNs, LOOKAHEAD_NS);
    const int64_t refillNs = std::max<int64_t>(aheadNs / REFILLS_PER_LOOKAHEAD, POLL_NS);

    // song time -> Maestro clock. timelineNs == ticks == wall, all sampled together.
    struct Anchor
    {
        int64_t timelineNs = 0;
        MaestroTime ticks = 0;
        Clock::time_point wall;
    } anchor;

    bool synced = false;
    uint64_t curEpoch = 0;
    int64_t nextPlayNs = 0;

    auto toTicks = [&](int64_t ns) -> MaestroTime
    {
        const int64_t delta = ns - anchor.timelineNs;
        if (delta <= 0)
            return anchor.ticks;
        return anchor.ticks + rt->ns_to_ticks(static_cast<uint64_t>(delta));
    };

    auto idleWait = [&](int64_t ns)
    {
        std::unique_lock<std::mutex> lock(scheduleMutex);
        scheduleCV.wait_for(lock, std::chrono::nanoseconds(ns), [this] {
            return stopFlag.load();
        });
    };

    auto fail = [&]
    {
        stopFlag.store(true);
        scheduleCV.notify_all();
        spaceCV.notify_all();
    };

    // Drops everything queued in Maestro and in the ring, re-anchors on the timer's
    // current position and tells the generator to start there.
    auto resync = [&]
    {
        rt->reset(inst);

        // everything produced so far is stale
        tail.store(head.load(std::memory_order_acquire), std::memory_order_release);

        anchor.timelineNs = SecondsToNs(timer->Elapsed());
        anchor.wall = Clock::now();
        anchor.ticks = rt->now() + rt->ns_to_ticks(static_cast<uint64_t>(latencyNs));

        {
            std::lock_guard<std::mutex> lock(ctlMutex);
            ctlSeekNs = anchor.timelineNs;
            ++ctlEpoch;
            curEpoch = ctlEpoch;
            epoch.store(ctlEpoch, std::memory_order_release);
        }
        spaceCV.notify_all();

        synced = true;
        nextPlayNs = anchor.timelineNs;   // first window is due immediately
    };

    while (!stopFlag.load())
    {
        // paused / stopped timer: nothing may be queued, Maestro would play it on its own
        if (!timer->IsRunning() || timer->IsPaused())
        {
            if (synced)
            {
                rt->reset(inst);
                synced = false;
            }
            idleWait(POLL_NS);
            continue;
        }

        int64_t nowNs = SecondsToNs(timer->Elapsed());
        auto wallNow = Clock::now();

        if (synced)
        {
            const int64_t predicted = anchor.timelineNs +
                std::chrono::duration_cast<std::chrono::nanoseconds>(wallNow - anchor.wall).count();

            if (std::llabs(nowNs - predicted) > JUMP_NS)
                synced = false;   // timer was moved
        }

        if (!synced)
        {
            resync();
            nowNs = anchor.timelineNs;
            wallNow = anchor.wall;
        }

        bool behind = false;

        if (nowNs >= nextPlayNs)
        {
            const int64_t endNs = nowNs + aheadNs;
            const uint32_t ep = static_cast<uint32_t>(curEpoch);

            // read "done" before head: if it is set, head already includes the final push
            const bool genDone = genDoneEpoch.load(std::memory_order_acquire) == curEpoch;
            const size_t h = head.load(std::memory_order_acquire);
            size_t t = tail.load(std::memory_order_relaxed);

            // Only events strictly before the newest published event are guaranteed
            // complete, unless the generator has finished the song.
            int64_t limit = endNs;
            if (!genDone)
            {
                if (h == t)
                {
                    limit = NS_MIN;
                }
                else
                {
                    const SchedEvent& last = ring[(h - 1) & RING_MASK];
                    limit = last.epoch == ep ? std::min(endNs, last.ns) : NS_MIN;
                }
                behind = limit < endNs;
            }

            while (t != h)
            {
                const SchedEvent& ev = ring[t & RING_MASK];

                if (ev.epoch != ep)   // stale, from before the last seek / pause
                {
                    ++t;
                    continue;
                }

                if (ev.ns >= limit)
                    break;

                rt->send_short(inst, 0, ev.message, toTicks(ev.ns));

                ++t;
            }

            tail.store(t, std::memory_order_release);
            spaceCV.notify_one();

#ifdef MAESTRO_DEBUG_BATCH
            if (behind)
                std::cerr << "[maestro] generator is behind the player\n";
#endif

            // if the generator is behind, retry soon instead of waiting a full interval
            nextPlayNs = behind ? nowNs : nowNs + refillNs;
        }

        const int64_t timelineNow = nowNs +
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - wallNow).count();
        const int64_t remaining = nextPlayNs - timelineNow;

        if (remaining > 0)
            idleWait(std::min(remaining, POLL_NS));
        else if (behind)
            idleWait(1'000'000);
    }
}

void MaestroEngine::RenderSettings()
{
    if (!hasMaestro) return;
    if (ImGui::Button("Open Maestro settings"))
    {
        rt->show_settings(inst);
    }
}