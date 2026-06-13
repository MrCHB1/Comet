#pragma once

#include "../Events/MIDIEvent.h"
#include "../Events/NoteEvent.h"
#include "MIDI/TempoMap.h"
#include "MIDI/MIDIDefs.h"
#include <vector>
#include <algorithm>
#include <iterator>

class SequenceFuncs
{
public:
    template <typename T>
    static std::vector<T> MergeSequences(
        std::vector<T>&& seq1,
        std::vector<T>&& seq2)
    {
        static_assert(std::is_base_of_v<MIDIEvent, T>, "T must derive from MIDIEvent.");

        if (seq1.empty()) return std::move(seq2);
        if (seq2.empty()) return std::move(seq1);

        std::vector<T> result;
        result.reserve(seq1.size() + seq2.size());

        T* s1ptr = seq1.data();
        T* s1end = s1ptr + seq1.size();
        T* s2ptr = seq2.data();
        T* s2end = s2ptr + seq2.size();

        while (s1ptr != s1end && s2ptr != s2end)
        {
            if (s1ptr->tick <= s2ptr->tick)
                result.emplace_back(std::move(*s1ptr++));
            else
                result.emplace_back(std::move(*s2ptr++));
        }

        if (s1ptr != s1end)
            result.insert(result.end(), std::make_move_iterator(s1ptr), std::make_move_iterator(s1end));
        if (s2ptr != s2end)
            result.insert(result.end(), std::make_move_iterator(s2ptr), std::make_move_iterator(s2end));

        return result;
    }

    template <typename T>
    static std::vector<T> FlattenSequence(std::vector<std::vector<T>>&& tracks)
    {
        static_assert(std::is_base_of_v<MIDIEvent, T>, "T must derive from MIDIEvent.");

        if (tracks.empty()) return {};
        if (tracks.size() == 1) return std::move(tracks[0]);
        if (tracks.size() == 2) return MergeSequences(std::move(tracks[0]), std::move(tracks[1]));

        size_t totalSize = 0;
        for (const auto& track : tracks)
            totalSize += track.size();

        if (totalSize == 0) return {};

        std::vector<T> result;
        result.reserve(totalSize);

        struct TrackHead
        {
            T* ptr;
            T* end;
            size_t trackIdx;

            bool operator<(const TrackHead& other) const
            {
                if (ptr->tick != other.ptr->tick)
                    return ptr->tick > other.ptr->tick;
                return trackIdx > other.trackIdx;
            }
        };

        std::vector<TrackHead> heap;
        heap.reserve(tracks.size());

        for (size_t i = 0; i < tracks.size(); i++)
        {
            if (tracks[i].empty()) continue;
            heap.push_back({ tracks[i].data(), tracks[i].data() + tracks[i].size(), i });
        }

        std::make_heap(heap.begin(), heap.end());

        while (!heap.empty())
        {
            std::pop_heap(heap.begin(), heap.end());
            TrackHead& head = heap.back();

            result.emplace_back(std::move(*head.ptr));
            head.ptr++;

            if (head.ptr != head.end)
                std::push_heap(heap.begin(), heap.end());
            else
                heap.pop_back();
        }

        return result;
    }

    template <typename T>
    static void ApplyTempoEvents(uint16_t ppq, TempoMap* tempoMap, std::vector<T>& events)
    {
        static_assert(std::is_base_of_v<MIDIEvent, T>, "T must derive from MIDIEvent.");

        const double MAX_SAFE_SECS = 42949.0;
    
        for (auto& ev : events)
        {
            long tick = ev.tick;
            double secs = tempoMap->TicksToSecsFromMap(ppq, ev.tick);
            ev.tick = static_cast<long>(std::min(secs, MAX_SAFE_SECS) * TIME_BASED_MULTIPLIER);
            if constexpr (std::is_same_v<T, NoteEvent>)
            {
                double endSecs = tempoMap->TicksToSecsFromMap(ppq, tick + ev.gate);
                double gate = endSecs - secs;
                ev.gate = static_cast<long>(gate * TIME_BASED_MULTIPLIER);
            }
        }
    }

    static std::vector<std::vector<NoteEvent>> DistributeNotes(std::vector<NoteEvent>&& notes);
};